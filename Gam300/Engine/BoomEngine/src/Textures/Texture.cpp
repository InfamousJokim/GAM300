#include "Core.h"
#include "Graphics/Textures/Texture.h"
#include "GlobalConstants.h"
#include "BoomProperties.h"
#include "Auxiliaries/AssetLoadContext.h"

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

	// NEW: Constructor for two-phase loading (uploads pre-loaded data to GPU)
	Texture2D::Texture2D(const TextureLoadContext& context)
		: Texture2D()
	{
		// Copy properties
		this->isCompileAsCompressed = context.isCompileAsCompressed;
		this->quality = context.quality;
		this->alphaThreshold = context.alphaThreshold;
		this->mipLevel = context.mipLevel;
		this->isGamma = context.isGamma;

		// Upload to GPU
		if (context.isCompressed) {
			UploadCompressedToGPU(context);
		}
		else {
			UploadUnCompressedToGPU(context);
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

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

	// NEW: Static CPU-side loading (no OpenGL calls - can run on worker thread)
	void Texture2D::LoadFromDiskCPU(const std::string& filename, TextureLoadContext& outContext) {
		outContext.filePath = filename;

		// Determine file extension
		size_t pos = filename.find_last_of('.');
		std::string ext;
		if (pos != std::string::npos && pos < filename.length() - 1) {
			ext = filename.substr(pos + 1);
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		}

		// Load compressed (DDS) or uncompressed
		if (ext == "dds") {
			// Load DDS using GLI
			gli::texture texture = gli::load(filename);
			if (texture.empty()) {
				throw std::runtime_error("gli::load(" + filename + ") failed.");
			}

			if (texture.target() != gli::TARGET_2D) {
				throw std::runtime_error("Texture is not 2D: " + filename);
			}

			gli::texture2d tex2D(texture);
			outContext.width = (int32_t)tex2D.extent().x;
			outContext.height = (int32_t)tex2D.extent().y;
			outContext.isCompressed = true;

			// Copy the entire GLI texture data
			size_t totalSize = tex2D.size();
			outContext.pixelData.resize(totalSize);
			memcpy(outContext.pixelData.data(), tex2D.data(), totalSize);

			// Store format info in channels field (we'll interpret it later)
			gli::gl gl(gli::gl::PROFILE_GL33);
			gli::gl::format format = gl.translate(texture.format(), texture.swizzles());
			outContext.channels = (int32_t)format.Internal; // Store as format ID
		}
		else {
			// Load uncompressed (PNG, JPG, HDR, etc.)
			bool isHDR = (ext == "hdr");
			outContext.isHDR = isHDR;
			outContext.isCompressed = false;

			if (isHDR) {
				int32_t channels = 0;
				float* pixels = stbi_loadf(filename.c_str(), &outContext.width, &outContext.height, &channels, 0);
				if (!pixels) {
					throw std::runtime_error("stbi_loadf(" + filename + ") failed.");
				}

				outContext.channels = channels;
				size_t dataSize = outContext.width * outContext.height * channels * sizeof(float);
				outContext.pixelData.resize(dataSize);
				memcpy(outContext.pixelData.data(), pixels, dataSize);
				stbi_image_free(pixels);
			}
			else {
				uint8_t* pixels = stbi_load(filename.c_str(), &outContext.width, &outContext.height, nullptr, 4);
				if (!pixels) {
					throw std::runtime_error("stbi_load(" + filename + ") failed.");
				}

				outContext.channels = 4;
				size_t dataSize = outContext.width * outContext.height * 4;
				outContext.pixelData.resize(dataSize);
				memcpy(outContext.pixelData.data(), pixels, dataSize);
				stbi_image_free(pixels);
			}
		}
	}

	// NEW: Upload uncompressed texture data to GPU (main thread only)
	void Texture2D::UploadUnCompressedToGPU(const TextureLoadContext& context) {
		width = context.width;
		height = context.height;

		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);

		if (context.isHDR) {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, context.pixelData.data());
		}
		else {
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, context.pixelData.data());
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glGenerateMipmap(GL_TEXTURE_2D);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// NEW: Upload compressed texture data to GPU (main thread only)
	void Texture2D::UploadCompressedToGPU(const TextureLoadContext& context) {
		width = context.width;
		height = context.height;

		// For compressed textures (DDS), reload from disk on main thread
		// DDS files are already fast to load since they're pre-compressed
		gli::texture texture = gli::load(context.filePath);
		if (texture.empty()) {
			BOOM_ERROR("Failed to reload compressed texture: {}", context.filePath);
			return;
		}

		gli::texture2d tex(texture);
		gli::gl gl(gli::gl::PROFILE_GL33);
		gli::gl::format format = gl.translate(texture.format(), texture.swizzles());

		GLenum internalFormat;
		if (format.Internal == gli::gl::INTERNAL_RGBA_DXT1) {
			internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		}
		else if (format.Internal == gli::gl::INTERNAL_RGB_BP_UNORM) {
			internalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM;
		}
		else if (format.Internal == gli::gl::INTERNAL_RGB_BP_UNSIGNED_FLOAT) {
			internalFormat = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
		}
		else {
			throw std::exception("Unsupported compressed format");
		}

		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);

		bool isFailed = true;
		uint32_t failedCounter = 0;
		do {
			isFailed = false;
			for (size_t level = 0; level < tex.levels(); ++level) {
				glCompressedTexImage2D(
					GL_TEXTURE_2D,
					(GLint)level,
					internalFormat,
					(GLsizei)tex.extent(level).x,
					(GLsizei)tex.extent(level).y,
					0,
					(GLsizei)tex.size(level),
					tex.data(0, 0, level)
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
			throw std::exception("UploadCompressedToGPU() - glCompressedTexImage2D() failed.");
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex.levels() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(tex.levels() - 1));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		glBindTexture(GL_TEXTURE_2D, 0);
	}
}