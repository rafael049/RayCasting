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

	viewMatrix[2][0] = (float)(context.width/2);
	viewMatrix[2][1] = (float)(context.height/2);
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
	float tx = texture.width * uv.x;
	float ty = texture.height * uv.y;

	int txi = (int)tx;
	int tyi = (int)ty;

	auto getTexturePixelRepeated = [](const media::Image& texture, int x, int y) -> auto
		{
			int height = static_cast<int>(texture.height);
			int width = static_cast<int>(texture.width);
			return texture.data[(y % height) * width + (x % width)];
		};

	auto pa = getTexturePixelRepeated(texture, txi, tyi);
	auto pb = getTexturePixelRepeated(texture, txi + 1, tyi);
	auto pc = getTexturePixelRepeated(texture, txi, tyi + 1);
	auto pd = getTexturePixelRepeated(texture, txi + 1, tyi + 1);

	auto ab = pb * (tx - (float)txi) + pa * (1.0f - (tx - (float)txi));
	auto cd = pd * (tx - (float)txi) + pc * (1.0f - (tx - (float)txi));

	auto abcd = cd * (ty - tyi) + ab * (1 - (ty - tyi));

	return abcd; // getTexturePixelRepeated(texture, txi, tyi);
}

size_t getMipmapLevel(float distance)
{
	if (distance < 25.0f)
	{
		return 0;
	}
	if (distance < 50.0f)
	{
		return 1;
	}
	if (distance < 100.0f)
	{
		return 2;
	}
	if (distance < 200.0f)
	{
		return 3;
	}

	return 0;
}

auto renderWalls(rendering::Context& context, const camera::Camera& camera, const std::vector<wall::Wall>& level, const std::vector<rendering::Texture>& textures)
{
	std::vector<float> zbuffer(context.width, camera.farPlane);
	std::vector<glm::vec3> colorBuffer(context.width, glm::vec3(0.0f));
	std::vector<float> uvBuffer(context.width, 0.0f);

	const int numberOfRays = context.width;
	const auto rayOrigin = camera.position;
	const auto frontVector = camera.front;
	const auto rightVector = glm::vec2(frontVector.y, -frontVector.x);

	const float projectionPlaneHeight = std::tanf(camera.fov / 2) * 2;
	const float projectionPlaneWidth = projectionPlaneHeight * ((float)context.width / (float)context.height);
	const float rayVectorOffset = projectionPlaneWidth / numberOfRays;

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

				float eyeDistance = glm::distance(rayOrigin, intersectionPoint);

				float normalizedCameraPlaneDistance = eyeDistance * glm::dot(frontVector, rayDirection);

				if (normalizedCameraPlaneDistance < zbuffer[i])
				{
					zbuffer[i] = normalizedCameraPlaneDistance;
					colorBuffer[i] = wall.color;
					uvBuffer[i] = glm::distance(intersectionPoint, wall.line.start);
				}
			}
		}
	}

	for (int i = 0; i < zbuffer.size(); i++)
	{
		float pixelDistance = zbuffer[i];

		if (pixelDistance < camera.farPlane)
		{
			float worlWallTop = 2.0f - camera.height;
			float worlWallBottom = 0.0f - camera.height;
			float viewWallTop = worlWallTop / pixelDistance;
			float viewWallBottom = worlWallBottom / pixelDistance;
			int screenWallTop = viewWallTop * (float)(context.height) / projectionPlaneHeight;
			int screenWallBottom = viewWallBottom * (float)(context.height) / projectionPlaneHeight;

			float pixelHorizontalPostion = zbuffer.size() - (i + 1);


			size_t mipMapLevel = getMipmapLevel(pixelDistance);
			const auto& texture = textures[0].mipmaps[mipMapLevel * (int)context.useMipmap];

			for (int j = std::max(-(int)context.height/2, screenWallBottom) + 1; j < std::min((int)context.height/2, screenWallTop); j++)
			{
				const float uvY = camera.height + ((float)j * (projectionPlaneHeight / (float)context.height)) * pixelDistance;
				const glm::vec2 uv = glm::vec2(uvBuffer[i], uvY);
				const glm::vec3 color = sampleFromTexture(texture, uv);

				rendering::setSceenBufferPixel(context, static_cast<size_t>(pixelHorizontalPostion), context.height/2 - j, glm::vec4(color, 1.0f));
			}
		}
	}
}


auto renderFloorAndCeiling(rendering::Context& context, const camera::Camera& camera, const std::vector<rendering::Texture>& textures)
{

	const int screenCenterX = context.width / 2;
	const int screenCenterY = context.height / 2;

	const float eyeHeight = camera.height;
	const float projectionPlaneHeight = std::tanf(camera.fov / 2);
	const float projectionPlaneWidth = projectionPlaneHeight * ((float)context.width / (float)context.height);
	const size_t numberOfRays = context.height / 2;
	const float rayOffset = projectionPlaneHeight / numberOfRays;

	for (size_t i = 0; i < numberOfRays; i++)
	{
		const auto ray = glm::normalize(glm::vec2(1.0f, -(i * rayOffset))); // +x is forward and +y is up
		const auto downVec = glm::vec2(0.0f, -1.0f); // angle is measured from this vec
		const float angle = std::acos(glm::dot(downVec, ray));
		const float intersectionDistance = std::tanf(angle) * eyeHeight; // this is the distance of the intersection point between the ray and the floor


		size_t mipMapLevel = getMipmapLevel(intersectionDistance);
		const auto& floorTexture = textures[1].mipmaps[mipMapLevel * (int)context.useMipmap];

		for (int j = 0; j < context.width; j++)
		{
			const auto right = glm::vec2(camera.front.y, -camera.front.x);
			const float uvFrontFactor = intersectionDistance;
			const float uvRightFactor = (j - screenCenterX) * (projectionPlaneWidth / (float)(context.width / 2)) * intersectionDistance;
			const auto uv = camera.position + (camera.front * uvFrontFactor) + (right * uvRightFactor);

			const auto color = sampleFromTexture(floorTexture, uv);
			//const auto color = glm::vec4(0.6f, 0.1f, 0.1f, 1.0f);

			rendering::setSceenBufferPixel(context, j, i + screenCenterY, color);
		}
	}
}

auto renderMain(rendering::Context& context, const camera::Camera& camera, const std::vector<wall::Wall>& level, const std::vector<rendering::Texture>& textures)
{

	rendering::clearContext(context);

	renderFloorAndCeiling(context, camera, textures);
	renderWalls(context, camera, level, textures);

	rendering::renderContext(context);
}


auto processInput(SDL::EventHandler& eventHandler, camera::Camera& camera, rendering::Context& context, const float deltaTimeSecs)
{
		eventHandler.pollEvents();

		const float movementSensivity = 7.0f * deltaTimeSecs;
		float rotationSensivity = 2.0f * deltaTimeSecs;

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

		if (eventHandler.getKeyState(SDL::KeyCode::KEY_KP_PLUS) == SDL::KeyState::Holding)
		{
			camera.fov += 0.5f * deltaTimeSecs;
		}
		if (eventHandler.getKeyState(SDL::KeyCode::KEY_KP_MINUS) == SDL::KeyState::Holding)
		{
			camera.fov -= 0.5f * deltaTimeSecs;
		}

		if (eventHandler.getKeyState(SDL::KeyCode::KEY_Q) == SDL::KeyState::Holding)
		{
			camera.height += 0.5f * deltaTimeSecs;
		}
		if (eventHandler.getKeyState(SDL::KeyCode::KEY_E) == SDL::KeyState::Holding)
		{
			camera.height -= 0.5f * deltaTimeSecs;
		}

		if (eventHandler.getKeyState(SDL::KeyCode::KEY_M) == SDL::KeyState::Holding)
		{
			context.useMipmap = true;
		}
		if (eventHandler.getKeyState(SDL::KeyCode::KEY_N) == SDL::KeyState::Holding)
		{
			context.useMipmap = false;
		}
}


int main(int argc, char* argv[])
{
	std::vector<rendering::Texture> textures;

	auto result = media::imageFromBitMapFile("assets/textures/brick.bmp");

	if (!result.has_value())
	{
		std::cout << result.error();
		exit(1);
	}
	else
	{
		textures.push_back(rendering::createTexture(std::move(result.value())));
	}

	result = media::imageFromBitMapFile("assets/textures/mud.bmp");

	if (!result.has_value())
	{
		std::cout << result.error();
		exit(1);
	}
	else
	{
		textures.push_back(rendering::createTexture(std::move(result.value())));
	}


	SDL::initializeSDL();


	auto viewWindow = *SDL::createWindow("View window", { 800, 600 });
	auto mainWindow = *SDL::createWindow("Main window", { 800, 800 });

	auto viewRenderer = *SDL::createRenderer(viewWindow);
	auto mainRenderer = *SDL::createRenderer(mainWindow);

	rendering::Context viewContext(std::move(viewWindow), std::move(viewRenderer), 800, 600);
	rendering::Context mainContext(std::move(mainWindow), std::move(mainRenderer), 800, 800);

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

		processInput(eventHandler, camera, mainContext, deltaTimeSec);

		glm::mat4 cameraTrsf = camera::getTransform(camera);

		renderMain(mainContext, camera, lines, textures);

		renderViewport(viewContext, camera, lines);
	}

	return 0;
}

