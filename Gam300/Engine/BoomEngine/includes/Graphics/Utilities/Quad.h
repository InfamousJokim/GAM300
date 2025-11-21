#pragma once
#include "../Buffers/Mesh.h"

namespace Boom {
	using Quad3D = std::unique_ptr<Mesh<ShadedVert>>;
	using Quad2D = std::unique_ptr<Mesh<QuadVert>>;

	BOOM_INLINE Quad2D CreateTestQuad2D() {
		MeshData<QuadVert> data;

		data.vtx = {
			{{-.5f, -.5f}, {0.f, 0.f}},
			{{ .5f, -.5f}, {1.f, 0.f}},
			{{ .5f,  .5f}, {1.f, 1.f}},
			{{-.5f,  .5f}, {0.f, 1.f}}
		};
		data.idx = {
			3, 0, 2, 1
		};

		return std::make_unique<Mesh<QuadVert>>(std::move(data));
	}

	//Triangle Strip config
	BOOM_INLINE Quad2D CreateQuad2D() {
		MeshData<QuadVert> data;

		data.vtx = {
			{{-1.f, -1.f}, {0.f, 0.f}},
			{{ 1.f, -1.f}, {1.f, 0.f}},
			{{ 1.f,  1.f}, {1.f, 1.f}},
			{{-1.f,  1.f}, {0.f, 1.f}}
		};
		data.idx = {
			3, 0, 2, 1
		};

		return std::make_unique<Mesh<QuadVert>>(std::move(data));
	}

	BOOM_INLINE Quad3D CreateQuad3D() {
		MeshData<ShadedVert> data;

		ShadedVert v0, v1, v2, v3;
		//anti-clockwise
		v0.pos = { -0.5f, -0.5f, 0.f };
		v1.pos = { 0.5f, -0.5f, 0.f };
		v2.pos = { 0.5f, 0.5f, 0.f };
		v3.pos = { -0.5f, 0.5f, 0.f };

		//all norm similar
		v3.norm = v2.norm = v1.norm = v0.norm = { 0.f, 0.f, 1.f };

		//texture coords
		v0.uv = {};
		v1.uv = { 1.f, 0.f };
		v2.uv = { 1.f, 1.f };
		v3.uv = { 0.f, 1.f };

		//copy to data structure
		data.vtx.push_back(v0);
		data.vtx.push_back(v1);
		data.vtx.push_back(v2);
		data.vtx.push_back(v3);

		data.idx = {
			0, 1, 2,
			0, 2, 3
		};

		return std::make_unique<Mesh<ShadedVert>>(std::move(data));
	}
}