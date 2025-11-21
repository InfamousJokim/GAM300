#include "Core.h"
#include "Graphics/Textures/Texture.h"
#include "GlobalConstants.h"
#include "BoomProperties.h"

#pragma warning(push)
#pragma warning(disable : 4244 4267 4458 4100 5054 4189 26819 6262 26495) //library warnings ignored
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <gli/gli.hpp>
#pragma warning(pop)

#include <compressonator.h>

namespace Boom {
	Texture2D::Texture2D() : height{}, width{}, id{}
		, isCompileAsCompressed{ true }
		, quality{ 0.5f }
		, alphaThreshold{ 128 }
		, mipLevel{ 10 }
		, isGamma{ true }
	{
	}

	Texture2D::Texture2D(std::string const& filename)
		: Texture2D()
	{
		try {
			std::string ext{ GetExtension(filename) };
			if (ext == "dds") {
				LoadCompressed(filename);
			}
			else {
				LoadUnCompressed(filename);
			}
		}
		catch (std::exception e) {
			char const* tmp{ e.what() };
			BOOM_ERROR("ERROR_Texture2D({}): {}", filename, tmp);
		}
	}

	Texture2D::~Texture2D() {
		if (id != 0) {
			glDeleteTextures(1, &id);
		}
	}

	void Texture2D::LoadUnCompressed(std::string const& filename) {
		bool isHDR{ GetExtension(filename) == "hdr" };

		//texture data
		void* pixels{};
		if (isHDR) {
			int32_t channels{};
			pixels = stbi_loadf(filename.c_str(), &width, &height, &channels, 0);
		}
		else {
			pixels = stbi_load(filename.c_str(), &width, &height, nullptr, 4);
		}

		if (pixels == nullptr) {
			throw std::exception(("stbi_load(" + filename + ") failed.").c_str());
		}

		//texture buffers
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		if (isHDR) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, (float*)pixels);
		}
		else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		}
		stbi_image_free(pixels);

		//options
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glGenerateMipmap(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, 0);
	}
	void Texture2D::LoadCompressed(std::string const& filename) {
		// Load DDS using GLI
		gli::texture texture = gli::load(filename);
		if (texture.empty()) {
			throw std::exception(("gli::load(" + filename + ") failed.").c_str());
		}

		// Ensure it's a 2D texture and DXT1 format
		if (texture.target() != gli::TARGET_2D) {
			throw std::exception("texture.target != TARGET_2D");
		}
		gli::gl gl(gli::gl::PROFILE_GL33); // Adjust for your OpenGL version
		gli::gl::format format = gl.translate(texture.format(), texture.swizzles());

		GLenum internalFormat;
		if (format.Internal == gli::gl::INTERNAL_RGBA_DXT1) { //BC1 or DXT1
			internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		}
		else if (format.Internal == gli::gl::INTERNAL_RGB_BP_UNORM) { //BC7
			internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM;
		}
		else if (format.Internal == gli::gl::INTERNAL_RGB_BP_UNSIGNED_FLOAT) { //HDR/EXR
			internalFormat = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
		}
		else {
			throw std::exception(("gli::gl::format UNKNOWN - supported:(BC1/DXT1, BC7, BC6H)"));
		}

		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);

		gli::texture2d tex2D(texture);
		width = (int32_t)tex2D.extent().x;
		height = (int32_t)tex2D.extent().y;

		//load textures according to mipmap levels
		bool isFailed{true};
		uint32_t failedCounter{};
		do {
			isFailed = false;
			for (size_t level{}; level < tex2D.levels(); ++level) {
				glCompressedTexImage2D(
					GL_TEXTURE_2D,
					(GLint)level,
					internalFormat,
					(GLsizei)tex2D.extent(level).x,
					(GLsizei)tex2D.extent(level).y,
					0,
					(GLsizei)tex2D.size(level),
					tex2D.data(0, 0, level)
				);

				if (glGetError() != GL_NO_ERROR) {
					glDeleteTextures(1, &id);
					isFailed = true;
					++failedCounter;
					break;
				}
			}
		} while (isFailed && failedCounter < 10);
		if (failedCounter == 10) {
			glDeleteTextures(1, &id);
			throw std::exception("LoadCompressed() - glCompressedTexImage2D() failed.");
		}

		//textures has different options if they have multiple levels of mipmaps
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex2D.levels() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(tex2D.levels() - 1));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	//set's texture's active unit and uniform to graphics
	void Texture2D::Use(int32_t uniform, int32_t unit) {
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, id);
		glUniform1i(uniform, unit);
	}

	void Texture2D::Bind() {
		glBindTexture(GL_TEXTURE_2D, id);
	}
	void Texture2D::UnBind() {
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	Texture2D::operator uint32_t() const noexcept { return id; }
	int32_t Texture2D::Height() const noexcept { return height; }
	int32_t Texture2D::Width() const noexcept { return width; }

	std::string Texture2D::GetExtension(std::string const& filename) {
		uint32_t pos{ (uint32_t)filename.find_last_of('.') };
		if (pos == std::string::npos || pos == filename.length() - 1) {
			return ""; //no extension
		}
		std::string ext{ filename.substr(pos + 1) };
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); //lowercase
		return ext;
	}

	bool Texture2D::IsHDR(std::string const& filename) {
		gli::texture texture = gli::load(filename);
		if (texture.empty()) {
			throw std::exception(("gli::load(" + filename + ") failed.").c_str());
		}

		// Ensure it's a 2D texture and DXT1 format
		if (texture.target() != gli::TARGET_2D) {
			throw std::exception("texture.target != TARGET_2D");
		}
		gli::gl gl(gli::gl::PROFILE_GL33); // Adjust for your OpenGL version
		gli::gl::format format = gl.translate(texture.format(), texture.swizzles());

		return format.Internal == gli::gl::INTERNAL_RGB_BP_UNSIGNED_FLOAT; //check HDR/EXR format encoded
	}
}