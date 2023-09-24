#pragma once

#include <iostream>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>

namespace camera
{
	struct Camera
	{
		glm::vec2 position = glm::vec2(0.0f, 0.0f);

		glm::vec2 front = glm::vec2(0.0f, 1.0f);

		float fov = glm::radians(90.0f);

		float farPlane = 100.0f;
	};


	auto getTransform(const Camera& camera) -> glm::mat3
	{
		auto id = glm::mat3(1.0f);

		auto translated = glm::translate(id, camera.position);

		auto left = glm::vec2(camera.front.y, -camera.front.x);

		translated[0][0] = left.x;
		translated[0][1] = left.y;
		translated[1][0] = camera.front.x;
		translated[1][1] = camera.front.y;

		return translated;
	}
}
