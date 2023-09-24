#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "line.hpp"


namespace wall
{


	struct Wall
	{
		Line line;

		float height;

		glm::vec3 color;

		Wall(glm::vec2 start, glm::vec2 end, float height, glm::vec3 color) :
			line{ start, end }, height{ height }, color{ color }
		{
		}
	};
}
