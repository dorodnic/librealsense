#pragma once

#include "rendering.h"

#include <vector>

namespace rs2
{
    struct obj_mesh
    {
        std::string         name;
        std::vector<int3>   indexes;
        std::vector<float3> positions;
        std::vector<float3> normals;
        std::vector<float2> uvs;
        std::vector<float3> tangents;

        void calculate_tangents();
    };

    inline obj_mesh make_grid(int a, int b, float x, float y)
	{
		obj_mesh res;

		auto toidx = [&](int i, int j) {
			return i * b + j;
		};

        for (auto i = 0; i < a; i++)
		{
			for (auto j = 0; j < b; j++)
			{
				float3 point{ (i * x) - (a * x) / 2.f,
							  (j * y) - (b * y) / 2.f, 
                              1.f};
				res.positions.push_back(point);
				res.normals.push_back(float3{ 0.f, 0.f, -1.f });

				res.uvs.emplace_back(float2{(float)j / b, (float)i / a});

				if (i < a - 1 && j < b - 1)
				{
					auto curr = toidx(i, j);
					auto next_a = toidx(i + 1, j);
					auto next_b = toidx(i, j + 1);
					auto next_ab = toidx(i + 1, j + 1);
					res.indexes.emplace_back(int3{ curr, next_a, next_b });
					res.indexes.emplace_back(int3{ next_a, next_ab, next_b } );
				}
			}
		}

		return res;
	}
}