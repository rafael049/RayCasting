#include <vector>
#include <numbers>
#include <thread>
#include <optional>
#include <ranges>

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



auto orientation(const ds::Vec2 p, const ds::Vec2 q, const ds::Vec2 r) -> Orientation
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

auto getIntersectionPoint(const Line& l1, const Line& l2) -> std::optional<ds::Vec2>
{
	if (not hasIntersection(l1, l2))
	{
		return std::nullopt;
	}

	ds::Vec2 p = l1.start;
	ds::Vec2 r = glm::normalize(l1.end - l1.start);

	ds::Vec2 q = l2.start;
	ds::Vec2 s = glm::normalize(l2.end - l2.start);

	const auto cross2D = [](ds::Vec2 a, ds::Vec2 b) -> float
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

auto applyTransform2d(const glm::mat3 transf, const ds::Vec2 vec) -> ds::Vec2
{
	ds::Vec3 v3 = transf * ds::Vec3(vec, 1.0f);

	return ds::Vec2(v3.x, v3.y);
}


auto renderViewport(rendering::Context& context, const camera::Camera& camera, const std::vector<wall::Wall>& level)
{
	glm::mat3 viewMatrix = glm::mat3(50.0f);

	viewMatrix[2][0] = (float)(context.width/2);
	viewMatrix[2][1] = (float)(context.height/2);
	viewMatrix[1][1] *= -1.0f;

	const Line cameraLine = Line{ camera.position, camera.position + camera.front * camera.farPlane };

	SDL::renderClear(context.renderer, ds::ColorRGBA(0, 0, 0, 255));

	SDL::drawLine(context.renderer, applyTransform2d(viewMatrix, cameraLine.start), applyTransform2d(viewMatrix, cameraLine.end), ds::ColorRGBA(255, 0, 0, 255));

	for (const auto& wall : level)
	{
		SDL::drawLine(context.renderer, applyTransform2d(viewMatrix, wall.line.start), applyTransform2d(viewMatrix, wall.line.end), ds::ColorRGBA(wall.color, 255));
	}

	SDL::renderPresent(context.renderer);
}


auto sampleFromTexture(const media::Image& texture, const ds::Vec2 uv, const bool useFiltering) -> ds::ColorRGBA
{
	float uvx = (uv.x - (int64_t)uv.x);
	float uvy = (uv.y - (int64_t)uv.y);

	if (uvx < 0.0f)
	{
		uvx += 1.0f;
	}
	if (uvy < 0.0f)
	{
		uvy += 1.0f;
	}

	const float tx = texture.width * uvx;
	const float ty = texture.height * uvy;

	const size_t txi = static_cast<size_t>(tx);
	const size_t tyi = static_cast<size_t>(ty);

	auto getTexturePixelRepeated = [](const media::Image& texture, size_t x, size_t y, ds::ColorRGB& outColor)
		{
			outColor = texture.data[(y % texture.height) * texture.width + (x % texture.width)];
		};

	if (useFiltering == false)
	{
		ds::ColorRGB color{};
		getTexturePixelRepeated(texture, txi, tyi, color);

		return { color[0], color[1], color[2], 255};
	}

	ds::ColorRGB pa{};
	getTexturePixelRepeated(texture, txi, tyi, pa);
	ds::ColorRGB pb{};
	getTexturePixelRepeated(texture, txi + 1, tyi, pb);
	ds::ColorRGB pc{};
	getTexturePixelRepeated(texture, txi, tyi + 1, pc);
	ds::ColorRGB pd{};
	getTexturePixelRepeated(texture, txi + 1, tyi + 1, pd);

	const int w1 = static_cast<int>(255.0f*(tx - (float)txi));
	const int w2 = 255 - w1;

	const std::array<int, 4> ab = {
		(pb[0] * w1) + (pa[0] * w2),
		(pb[1] * w1) + (pa[1] * w2),
		(pb[2] * w1) + (pa[2] * w2)
	};
	const std::array<int, 4> cd = {
		(pd[0] * w1) + (pc[0] * w2),
		(pd[1] * w1) + (pc[1] * w2),
		(pd[2] * w1) + (pc[2] * w2) 
	};

	const int w3 = static_cast<int>(255.0f*(ty - (float)tyi));
	const int w4 = 255 - w3;
	const std::array<int, 4> abcd = {
		((cd[0] * w3) + (ab[0] * w4)) / (255*255),
		((cd[1] * w3) + (ab[1] * w4)) / (255*255),
		((cd[2] * w3) + (ab[2] * w4)) / (255*255)
	};

	return ds::ColorRGBA(abcd[0], abcd[1], abcd[2], 255); // getTexturePixelRepeated(texture, txi, tyi);
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
	std::vector<ds::Vec3> colorBuffer(context.width, ds::Vec3(0.0f));
	std::vector<float> uvBuffer(context.width, 0.0f);

	const int numberOfRays = context.width;
	const auto rayOrigin = camera.position;
	const auto frontVector = camera.front;
	const auto rightVector = ds::Vec2(frontVector.y, -frontVector.x);

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
				const ds::Vec2 uv = ds::Vec2(uvBuffer[i], uvY);
				const ds::Vec3 color = sampleFromTexture(texture, uv, context.useFiltering);

				rendering::setSceenBufferPixel(context, static_cast<size_t>(pixelHorizontalPostion), context.height/2 - j, ds::ColorRGBA(color, 1.0f));
				rendering::setStencilBufferPixel(context, static_cast<size_t>(pixelHorizontalPostion), context.height/2 - j, 1);
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
		const auto ray = glm::normalize(ds::Vec2(1.0f, -(i * rayOffset))); // +x is forward and +y is up
		const auto downVec = ds::Vec2(0.0f, -1.0f); // angle is measured from this vec
		const float angle = std::acos(glm::dot(downVec, ray));
		const float intersectionDistance = std::tanf(angle) * eyeHeight; // this is the distance of the intersection point between the ray and the floor


		size_t mipMapLevel = getMipmapLevel(intersectionDistance);
		const auto& floorTexture = textures[1].mipmaps[mipMapLevel * (int)context.useMipmap];

		for (int j = 0; j < context.width; j++)
		{
			if (context.stencilBuffer[(i + screenCenterY) * context.width + j] == 1)
			{
				continue;
			}

			const auto right = ds::Vec2(camera.front.y, -camera.front.x);
			const float uvFrontFactor = intersectionDistance;
			const float uvRightFactor = (j - screenCenterX) * (projectionPlaneWidth / (float)(context.width / 2)) * intersectionDistance;
			const auto uv = camera.position + (camera.front * uvFrontFactor) + (right * uvRightFactor);

			const auto color = sampleFromTexture(floorTexture, uv, context.useFiltering);
			//const auto color = ds::ColorRGBA(0.6f, 0.1f, 0.1f, 1.0f);

			rendering::setSceenBufferPixel(context, j, i + screenCenterY, color);
		}
	}
}

auto renderMain(rendering::Context& context, const camera::Camera& camera, const std::vector<wall::Wall>& level, const std::vector<rendering::Texture>& textures)
{

	rendering::clearContext(context);

	renderWalls(context, camera, level, textures);
	renderFloorAndCeiling(context, camera, textures);

	rendering::renderContext(context);
}


auto processInput(SDL::EventHandler& eventHandler, camera::Camera& camera, rendering::Context& context, const float deltaTimeSecs)
{
		eventHandler.pollEvents();

		const float movementSensivity = 7.0f * deltaTimeSecs;
		float rotationSensivity = 2.0f * deltaTimeSecs;

		const auto left = ds::Vec2(camera.front.y, -camera.front.x);

		ds::Vec2 direction(0.0f);

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
			camera.front = ds::Vec3(glm::rotateZ(ds::Vec3(camera.front, 0.0f), -rotationSensivity));
		}
		if (eventHandler.getKeyState(SDL::KeyCode::KEY_LEFT) == SDL::KeyState::Holding)
		{
			camera.front = ds::Vec3(glm::rotateZ(ds::Vec3(camera.front, 0.0f), rotationSensivity));
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

		if (eventHandler.getKeyState(SDL::KeyCode::KEY_B) == SDL::KeyState::Holding)
		{
			context.useFiltering = true;
		}
		if (eventHandler.getKeyState(SDL::KeyCode::KEY_P) == SDL::KeyState::Holding)
		{
			context.useFiltering = false;
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

	const int screenWidth = 800;
	const int screenHeight = 600;

	auto viewWindow = *SDL::createWindow("View window", { 800, 600 });
	auto mainWindow = *SDL::createWindow("Main window", { screenWidth, screenHeight });

	auto viewRenderer = *SDL::createRenderer(viewWindow);
	auto mainRenderer = *SDL::createRenderer(mainWindow);

	rendering::Context viewContext(std::move(viewWindow), std::move(viewRenderer), 800, 600);
	rendering::Context mainContext(std::move(mainWindow), std::move(mainRenderer), screenWidth, screenHeight);

	bool quit = false;
	auto eventHandler = SDL::EventHandler();

	std::vector<wall::Wall> lines = 
	{ 
		wall::Wall( ds::Vec2( 4.0f,  1.0f), ds::Vec2( 2.0f,  1.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( ds::Vec2( 2.0f,  1.0f), ds::Vec2( 2.0f,  3.0f), 1.0f, ds::Vec3(0.1f, 0.8f, 0.0f)),
		wall::Wall( ds::Vec2( 2.0f,  3.0f), ds::Vec2(-3.0f,  3.0f), 1.0f, ds::Vec3(0.1f, 0.0f, 0.8f)),
		wall::Wall( ds::Vec2(-3.0f,  3.0f), ds::Vec2(-3.0f, -1.0f), 1.0f, ds::Vec3(0.8f, 0.0f, 0.1f)),
		wall::Wall( ds::Vec2(-3.0f, -1.0f), ds::Vec2( 0.0f, -1.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( ds::Vec2( 0.0f, -1.0f), ds::Vec2( 0.0f, -2.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( ds::Vec2( 0.0f, -2.0f), ds::Vec2(-3.0f, -2.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( ds::Vec2(-3.0f, -2.0f), ds::Vec2(-3.0f, -4.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( ds::Vec2(-3.0f, -4.0f), ds::Vec2( 1.0f, -4.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( ds::Vec2( 2.0f, -4.0f), ds::Vec2( 3.0f, -4.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( ds::Vec2( 3.0f, -4.0f), ds::Vec2( 3.0f, -2.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( ds::Vec2( 3.0f, -2.0f), ds::Vec2( 2.0f, -2.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( ds::Vec2( 2.0f, -2.0f), ds::Vec2( 2.0f,  0.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall( ds::Vec2( 2.0f,  0.0f), ds::Vec2( 4.0f,  0.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
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

