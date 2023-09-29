#include <vector>
#include <numbers>
#include <thread>
#include <optional>
#include <ranges>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat2x2.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_transform_2d.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "sdl.hpp"
#include "camera.hpp"
#include "wall.hpp"
#include "media.hpp"
#include "rendering.hpp"


enum class Orientation
{
	ClockWise,
	CounterClockWise
};



auto orientation(const glm::vec2 p, const glm::vec2 q, const glm::vec2 r) -> Orientation
{
	float value = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);

	return value >= 0.0f ? Orientation::ClockWise : Orientation::CounterClockWise;
}

auto hasIntersection(const Line l1, const Line l2) -> bool
{
	bool condition1 = orientation(l1.start, l1.end, l2.start) != orientation(l1.start, l1.end, l2.end);
	bool condition2 = orientation(l2.start, l2.end, l1.start) != orientation(l2.start, l2.end, l1.end);

	return condition1 and condition2;
}

auto getIntersectionPoint(const Line& l1, const Line& l2) -> std::optional<glm::vec2>
{
	if (not hasIntersection(l1, l2))
	{
		return std::nullopt;
	}

	glm::vec2 p = l1.start;
	glm::vec2 r = glm::normalize(l1.end - l1.start);

	glm::vec2 q = l2.start;
	glm::vec2 s = glm::normalize(l2.end - l2.start);

	const auto cross2D = [](glm::vec2 a, glm::vec2 b) -> float
		{
			return a.x * b.y - a.y * b.x;
		};

	if (std::abs(cross2D(r, s)) > 0.001f)
	{
		float t = cross2D((q - p), s / cross2D(r, s));

		return p + t * r;
	}

	return std::nullopt;
}

auto applyTransform2d(const glm::mat3 transf, const glm::vec2 vec) -> glm::vec2
{
	glm::vec3 v3 = transf * glm::vec3(vec, 1.0f);

	return glm::vec2(v3.x, v3.y);
}


auto renderViewport(rendering::Context& context, const camera::Camera& camera, const std::vector<wall::Wall>& level)
{
	glm::mat3 viewMatrix = glm::mat3(50.0f);

	viewMatrix[2][0] = 400.0f;
	viewMatrix[2][1] = 300.0f;
	viewMatrix[1][1] *= -1.0f;

	const Line cameraLine = Line{ camera.position, camera.position + camera.front * camera.farPlane };

	SDL::renderClear(context.renderer, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

	SDL::drawLine(context.renderer, applyTransform2d(viewMatrix, cameraLine.start), applyTransform2d(viewMatrix, cameraLine.end), glm::vec4(1.0, 0.0, 0.0, 1.0));

	for (const auto& wall : level)
	{
		SDL::drawLine(context.renderer, applyTransform2d(viewMatrix, wall.line.start), applyTransform2d(viewMatrix, wall.line.end), glm::vec4(wall.color, 1.0f));
	}

	SDL::renderPresent(context.renderer);
}


auto sampleFromTexture(const media::Image& texture, const glm::vec2 uv) -> glm::vec4
{
	size_t pixelXPosition = ((size_t)(texture.width * uv.x)) % texture.width;
	size_t pixelYPostion = ((size_t)(texture.height* uv.y)) % texture.height;

	return texture.data[pixelYPostion * texture.width + pixelXPosition];
}


auto renderMain(rendering::Context& context, const camera::Camera& camera, const std::vector<wall::Wall>& level, const std::vector<media::Image>& textures)
{
	std::vector<float> zbuffer(800, 1.0f);
	std::vector<glm::vec3> colorBuffer(800, glm::vec3(0.0f));
	std::vector<float> uvBuffer(800, 0.0f);

	glm::mat4 cameraTrsf = camera::getTransform(camera);

	const int numberOfRays = 800;
	const auto rayOrigin = camera.position;
	const auto frontVector = camera.front;
	const auto rightVector = glm::vec2(frontVector.y, -frontVector.x);

	float projectionPlaneWidth = std::tanf(camera.fov / 2) * 2;
	float rayVectorOffset = projectionPlaneWidth / numberOfRays;


	for (size_t i = 0; i < numberOfRays; i++)
	{
		const float amountToOffset = (rayVectorOffset * (float)((int)i - (numberOfRays / 2)));
		const auto rayDirection = glm::normalize(frontVector - rightVector * amountToOffset);

		Line rayLine{ rayOrigin, rayOrigin + rayDirection * camera.farPlane };

		for (const auto& wall : level)
		{
			std::optional hasIntersection = getIntersectionPoint(rayLine, wall.line);

			if (hasIntersection.has_value())
			{
				auto intersectionPoint = *hasIntersection;

				// Get depth
				float eyeDistance = glm::distance(rayOrigin, intersectionPoint);

				float normalizedCameraPlaneDistance = eyeDistance * glm::dot(frontVector, rayDirection)  / camera.farPlane;

				if (normalizedCameraPlaneDistance < zbuffer[i])
				{
					zbuffer[i] = normalizedCameraPlaneDistance;
					colorBuffer[i] = wall.color;
					uvBuffer[i] = glm::distance(intersectionPoint, wall.line.start);
				}
			}
		}
	}

	rendering::clearContext(context);

	for (int i = 0; i < zbuffer.size(); i++)
	{
		float pixelDistance = zbuffer[i];

		if (pixelDistance < 1.0f && i % 1 == 0)
		{
			float lineHeight = 2.0f/pixelDistance;
			float pixelHorizontalPostion = zbuffer.size() - (i + 1);

			const Line verticalLine{ glm::vec2(pixelHorizontalPostion, 300.0f - lineHeight), glm::vec2(pixelHorizontalPostion, 300.0f + lineHeight) };

			auto& texture = textures.front();

			for (int j = std::max(-300, -(int)lineHeight); j < lineHeight && j < 300; j++)
			{
				glm::vec2 uv = glm::vec2(uvBuffer[i], ((float)j + lineHeight) / (2.0f * lineHeight));
				glm::vec3 color = sampleFromTexture(texture, uv);

				rendering::setSceenBufferPixel(context, pixelHorizontalPostion, 300 + j, glm::vec4(color, 1.0f));
			}
		}
	}

	rendering::renderContext(context);
}


auto processInput(SDL::EventHandler& eventHandler, camera::Camera& camera, const float deltaTimeSecs)
{
		eventHandler.pollEvents();

		const float movementSensivity = 7.0f * deltaTimeSecs;
		float rotationSensivity = 2.5f * deltaTimeSecs;

		const auto left = glm::vec2(camera.front.y, -camera.front.x);

		glm::vec2 direction(0.0f);

		if (eventHandler.getKeyState(SDL::KeyCode::KEY_W) == SDL::KeyState::Holding)
		{
			direction += camera.front;
		}
		if (eventHandler.getKeyState(SDL::KeyCode::KEY_S) == SDL::KeyState::Holding)
		{
			direction -= camera.front;
		}
		if (eventHandler.getKeyState(SDL::KeyCode::KEY_D) == SDL::KeyState::Holding)
		{
			direction += left;
		}
		if (eventHandler.getKeyState(SDL::KeyCode::KEY_A) == SDL::KeyState::Holding)
		{
			direction -= left;
		}

		if (glm::length(direction) > 0.1f)
		{
			camera.position += glm::normalize(direction) * movementSensivity;
		}


		if (eventHandler.getKeyState(SDL::KeyCode::KEY_RIGHT) == SDL::KeyState::Holding)
		{
			camera.front = glm::vec3(glm::rotateZ(glm::vec3(camera.front, 0.0f), -rotationSensivity));
		}
		if (eventHandler.getKeyState(SDL::KeyCode::KEY_LEFT) == SDL::KeyState::Holding)
		{
			camera.front = glm::vec3(glm::rotateZ(glm::vec3(camera.front, 0.0f), rotationSensivity));
		}
}


int main(int argc, char* argv[])
{
	std::vector<media::Image> textures;

	auto result = media::imageFromBitMapFile("assets/textures/brick.bmp");

	if (!result.has_value())
	{
		std::cout << result.error();
		exit(1);
	}
	else
	{
		textures.push_back(result.value());
	}


	SDL::initializeSDL();


	auto viewWindow = *SDL::createWindow("View window", { 800, 600 });
	auto mainWindow = *SDL::createWindow("Main window", { 800, 600 });

	auto viewRenderer = *SDL::createRenderer(viewWindow);
	auto mainRenderer = *SDL::createRenderer(mainWindow);

	rendering::Context viewContext(std::move(viewWindow), std::move(viewRenderer), 800, 600);
	rendering::Context mainContext(std::move(mainWindow), std::move(mainRenderer), 800, 600);

	bool quit = false;
	auto eventHandler = SDL::EventHandler();

	std::vector<wall::Wall> lines = 
	{ 
		wall::Wall( glm::vec2( 4.0f,  1.0f), glm::vec2( 2.0f,  1.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( glm::vec2( 2.0f,  1.0f), glm::vec2( 2.0f,  3.0f), 1.0f, glm::vec3(0.1f, 0.8f, 0.0f)),
		wall::Wall( glm::vec2( 2.0f,  3.0f), glm::vec2(-3.0f,  3.0f), 1.0f, glm::vec3(0.1f, 0.0f, 0.8f)),
		wall::Wall( glm::vec2(-3.0f,  3.0f), glm::vec2(-3.0f, -1.0f), 1.0f, glm::vec3(0.8f, 0.0f, 0.1f)),
		wall::Wall( glm::vec2(-3.0f, -1.0f), glm::vec2( 0.0f, -1.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( glm::vec2( 0.0f, -1.0f), glm::vec2( 0.0f, -2.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( glm::vec2( 0.0f, -2.0f), glm::vec2(-3.0f, -2.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( glm::vec2(-3.0f, -2.0f), glm::vec2(-3.0f, -4.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( glm::vec2(-3.0f, -4.0f), glm::vec2( 1.0f, -4.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( glm::vec2( 2.0f, -4.0f), glm::vec2( 3.0f, -4.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( glm::vec2( 3.0f, -4.0f), glm::vec2( 3.0f, -2.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( glm::vec2( 3.0f, -2.0f), glm::vec2( 2.0f, -2.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( glm::vec2( 2.0f, -2.0f), glm::vec2( 2.0f,  0.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( glm::vec2( 2.0f,  0.0f), glm::vec2( 4.0f,  0.0f), 1.0f, glm::vec3(0.8f, 0.1f, 0.0f)),
	};

	camera::Camera camera = camera::Camera();

	auto timeBefore = std::chrono::high_resolution_clock::now();
    auto timeNow = std::chrono::high_resolution_clock::now();

	while (!eventHandler.shouldQuit())
	{
		timeNow = std::chrono::high_resolution_clock::now();
		const float deltaTimeSec = std::chrono::duration_cast<std::chrono::milliseconds>(timeNow - timeBefore).count()/1000.0f;
		timeBefore = timeNow;

		processInput(eventHandler, camera, deltaTimeSec);

		glm::mat4 cameraTrsf = camera::getTransform(camera);

		int numberOfRays = 800;
		auto rayOrigin = applyTransform2d(cameraTrsf, glm::vec2(0.0f, 0.0f));
		auto front = camera.front;

		renderMain(mainContext, camera, lines, textures);

		renderViewport(viewContext, camera, lines);
	}

	return 0;
}

