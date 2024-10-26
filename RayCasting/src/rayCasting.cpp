#include <vector>
#include <numbers>
#include <thread>
#include <optional>
#include <ranges>
#include <numeric>
#include <algorithm>
#include <execution>
#include <functional>

#define GLM_ENABLE_EXPERIMENTAL

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


auto executeInParallel(size_t numThreads, size_t dataSize, std::function<void(size_t start, size_t end)> fn)
{
	std::vector<std::thread> threads(numThreads);

	int chunkSize = dataSize / numThreads;

	for (int i = 0; i < numThreads; ++i)
	{
		int start = i * chunkSize;
		int end = (i == numThreads - 1) ? dataSize : start + chunkSize;
		threads[i] = std::thread(fn, start, end);
	}

	for (auto& thread : threads) {
        thread.join();
    }
}



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

	viewMatrix[2][0] = (float)(context.width / 2);
	viewMatrix[2][1] = (float)(context.height / 2);
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

		return { color[0], color[1], color[2], 255 };
	}

	ds::ColorRGB pa{};
	getTexturePixelRepeated(texture, txi, tyi, pa);
	ds::ColorRGB pb{};
	getTexturePixelRepeated(texture, txi + 1, tyi, pb);
	ds::ColorRGB pc{};
	getTexturePixelRepeated(texture, txi, tyi + 1, pc);
	ds::ColorRGB pd{};
	getTexturePixelRepeated(texture, txi + 1, tyi + 1, pd);

	const int w1 = static_cast<int>(255.0f * (tx - (float)txi));
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

	const int w3 = static_cast<int>(255.0f * (ty - (float)tyi));
	const int w4 = 255 - w3;
	const std::array<int, 4> abcd = {
		((cd[0] * w3) + (ab[0] * w4)) / (255 * 255),
		((cd[1] * w3) + (ab[1] * w4)) / (255 * 255),
		((cd[2] * w3) + (ab[2] * w4)) / (255 * 255)
	};

	return ds::ColorRGBA(abcd[0], abcd[1], abcd[2], 255); // getTexturePixelRepeated(texture, txi, tyi);
}

size_t getMipmapLevel(float distance)
{
	if (distance < 10.0f)
	{
		return 0;
	}
	if (distance < 20.0f)
	{
		return 1;
	}
	if (distance < 40.0f)
	{
		return 2;
	}
	if (distance < 80.0f)
	{
		return 3;
	}

	return 0;
}

auto renderWalls(rendering::Context& context, const camera::Camera& camera, const std::vector<wall::Wall>& level, const std::vector<rendering::Texture>& textures)
{
	std::vector<ds::Vec3> colorBuffer(context.width, ds::Vec3(0.0f));
	std::vector<float> zbuffer(context.width, 1.0f);
	std::vector<float> uvBuffer(context.width, 0.0f);

	const int numberOfRays = context.width;
	const auto rayOrigin = camera.position;
	const auto frontVector = camera.front;
	const auto rightVector = ds::Vec2(frontVector.y, -frontVector.x);

	const float projectionPlaneHeight = std::tanf(camera.fov / 2) * 2;
	const float projectionPlaneWidth = projectionPlaneHeight * ((float)context.width / (float)context.height);
	const float rayVectorOffset = projectionPlaneWidth / numberOfRays;

	auto calculateWallZBuffer = [&](size_t start, size_t end)
		{
			for (size_t i = start; i < end; i++)
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

						float normalizedCameraPlaneDistance = eyeDistance * glm::dot(frontVector, rayDirection) / camera.farPlane;

						if (normalizedCameraPlaneDistance < zbuffer[i])
						{
							zbuffer[i] = normalizedCameraPlaneDistance;
							colorBuffer[i] = wall.color;
							uvBuffer[i] = glm::distance(intersectionPoint, wall.line.start);
						}
					}
				}
			}
		};
	//executeInParallel(12, numberOfRays, calculateWallZBuffer);
	calculateWallZBuffer(0, numberOfRays);

	auto render = [&](size_t start, size_t end)
		{
			for (size_t i = start; i < end; ++i)
			{
				float pixelDistance = zbuffer[i] * camera.farPlane;

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

					for (int j = std::max(-(int)context.height / 2, screenWallBottom) + 1; j < std::min((int)context.height / 2, screenWallTop); j++)
					{
						const float uvY = camera.height + ((float)j * (projectionPlaneHeight / (float)context.height)) * pixelDistance;
						const ds::Vec2 uv = ds::Vec2(uvBuffer[i], uvY);
						//const ds::Vec2 uv = ds::Vec2(uvY, uvBuffer[i]);
						const ds::Vec3 color = sampleFromTexture(texture, uv, context.useFiltering);

						rendering::setSceenBufferPixel(context, static_cast<size_t>(pixelHorizontalPostion), context.height / 2 - j, ds::ColorRGBA(color, 1.0f));
						rendering::setStencilBufferPixel(context, static_cast<size_t>(pixelHorizontalPostion), context.height / 2 - j, 1);
						rendering::setDepthBufferPixel(context, static_cast<size_t>(pixelHorizontalPostion), context.height / 2 - j, zbuffer[i]);
					}
				}
			}
		};

	executeInParallel(3, numberOfRays, render);

}


auto renderSprites(rendering::Context& context, const camera::Camera& camera, const std::vector<rendering::Sprite>& sprites)
{
	const int screenCenterX = context.width / 2;
	const int screenCenterY = context.height / 2;
	const float aspectRatio = (float)context.width / (float)context.height;

	const ds::Vec2 cameraRight = ds::Vec2(camera.front.y, -camera.front.x);

	for (const auto& sprite : sprites)
	{
		const float spriteCameraPlaneDistance = glm::dot(sprite.position - camera.position, camera.front);

		if (spriteCameraPlaneDistance <= 0.0f)
		{
			continue;
		}

		const float spriteSize = sprite.size / (spriteCameraPlaneDistance * std::tan(camera.fov / 2.0f));

		const float spriteWidth = spriteSize * context.height;
		const float spriteHeight = spriteSize * context.height;

		const ds::Vec2 fc = camera.position + camera.front * spriteCameraPlaneDistance;
		const ds::Vec2 fcSpriteVector = sprite.position - fc;
		const float distanceFc = glm::dot(fcSpriteVector, cameraRight);
		const float distanceFcScreen = (distanceFc / spriteCameraPlaneDistance);

		const int spriteScreenCenterX = distanceFcScreen * screenCenterX / (std::tan(camera.fov / 2) * aspectRatio) + screenCenterX;
		const int spriteScreenCenterY = ((camera.height + sprite.height + 0.0f) / spriteCameraPlaneDistance) * screenCenterY / std::tan(camera.fov / 2) + screenCenterY;
		const int spriteScreenLeft = spriteScreenCenterX - spriteWidth / 2;
		const int spriteScreenRight = spriteScreenCenterX + spriteWidth / 2;
		const int spriteScreenTop = spriteScreenCenterY - spriteHeight / 2;
		const int spriteScreenBottom = spriteScreenCenterY + spriteHeight / 2;

		for (int i = std::max(spriteScreenTop, 0); i < std::min(spriteScreenBottom, (int)context.height); i++)
		{
			for (int j = std::max(spriteScreenLeft, 0); j < std::min(spriteScreenRight, (int)context.width); j++)
			{
				if (context.depthBuffer[i * context.width + j] > spriteCameraPlaneDistance / camera.farPlane)
				{
					const auto uv = ds::Vec2((float)((j - spriteScreenLeft) / spriteWidth), -(float)((i - spriteScreenTop) / spriteHeight));
					const auto color = sampleFromTexture(sprite.texture.mipmaps[0], uv, false);

					if (color == ds::ColorRGBA(0, 255, 255, 255))
					{
						continue;
					}

					rendering::setSceenBufferPixel(context, j, i, color);
					rendering::setDepthBufferPixel(context, j, i, spriteCameraPlaneDistance / camera.farPlane);
				}
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

	auto render = [&](size_t start, size_t end)
		{
			for (size_t i = start; i < end; ++i)
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
		};

	executeInParallel(3, numberOfRays, render);
}

auto renderBackground(rendering::Context& context, const camera::Camera& camera, const std::vector<rendering::Texture>& textures)
{
	const int screenCenterX = context.width / 2;
	const int screenCenterY = context.height / 2;

	const float aspect = context.width / context.height;
	constexpr float pi = glm::pi<float>();
	constexpr float pi2 = 2 * pi;
	const media::Image& skyTexture = textures[4].mipmaps[0];
	const float textureAspect = (float)skyTexture.width / (float)skyTexture.height;

	auto render = [&](size_t start, size_t end)
		{
			for (int i = start; i < end; i++)
			{
				for (int j = 0; j < screenCenterX * 2; j++)
				{
					if (context.depthBuffer[i * context.width + j] < 1.0f)
					{
						continue;
					}

					const auto frontVector = ds::Vec3(camera.front.x, 0.0f, camera.front.y);
					const auto rightVector = ds::Vec3(camera.front.y, 0.0f, -camera.front.x);

					const float dx = ((float)(j - screenCenterX) / (float)screenCenterX);
					const float dy = ((float)((screenCenterY - i) / (float)screenCenterY));
					auto rayDir = frontVector + rightVector * (dx * std::atan(camera.fov / 2.0f) * aspect);
					rayDir.y = (dy * std::sin(camera.fov / 2.0f));

					rayDir = glm::normalize(rayDir);
					const float uvX = 0.5f + std::atan2(rayDir.z, rayDir.x) / pi2;
					const float uvY = 0.5f + std::asin(rayDir.y) / pi;
					const auto uv = ds::Vec2(uvX, uvY);
					const auto color = sampleFromTexture(skyTexture, uv, true);

					rendering::setSceenBufferPixel(context, j, i, color);
				}
			}
		};

	executeInParallel(6, screenCenterY, render);
}

auto renderMain(
	rendering::Context& context,
	const camera::Camera& camera,
	const std::vector<wall::Wall>& level,
	const std::vector<rendering::Texture>& textures,
	const std::vector<rendering::Sprite>& sprites)
{

	rendering::clearContext(context);

	renderWalls(context, camera, level, textures);
	renderFloorAndCeiling(context, camera, textures);
	renderSprites(context, camera, sprites);
	renderBackground(context, camera, textures);

	rendering::renderContext(context);
}


auto processInput(SDL::EventHandler& eventHandler, camera::Camera& camera, rendering::Context& context, const float deltaTimeSecs)
{
	eventHandler.pollEvents();

	const float movementSensivity = 7.0f * deltaTimeSecs;
	float rotationSensivity = 1.0f * deltaTimeSecs;

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
		camera.velocity = glm::normalize(direction) * movementSensivity;
	}


	if (eventHandler.getKeyState(SDL::KeyCode::KEY_RIGHT) == SDL::KeyState::Holding)
	{
		camera.angularVelocity = -rotationSensivity;
	}
	if (eventHandler.getKeyState(SDL::KeyCode::KEY_LEFT) == SDL::KeyState::Holding)
	{
		camera.angularVelocity = rotationSensivity;
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

auto loadTextures() -> std::vector<rendering::Texture>
{
	std::string filenames[] = {
		"assets/textures/brick.bmp",
		"assets/textures/mud.bmp",
		"assets/textures/coin.bmp",
		"assets/textures/tree1.bmp",
		"assets/textures/sky.bmp",
	};

	std::vector<rendering::Texture> textures;

	for (const auto& filename : filenames)
	{
		auto result = media::imageFromBitMapFile(filename);

		if (!result.has_value())
		{
			std::cout << result.error();
			exit(1);
		}
		else
		{
			textures.push_back(rendering::createTexture(std::move(result.value())));
		}
	}

	return textures;
}


int main(int argc, char* argv[])
{
	std::vector<rendering::Texture> textures = loadTextures();

	SDL::initializeSDL();

	const int screenWidth = 800;
	const int screenHeight = 600;

	auto mainWindow = *SDL::createWindow("Main window", { screenWidth, screenHeight });

	auto mainRenderer = *SDL::createRenderer(mainWindow);

	rendering::Context mainContext(std::move(mainWindow), std::move(mainRenderer), screenWidth, screenHeight);

	bool quit = false;
	auto eventHandler = SDL::EventHandler();

	std::vector<wall::Wall> walls =
	{
		wall::Wall(ds::Vec2(4.0f,  1.0f), ds::Vec2(2.0f,  1.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall(ds::Vec2(2.0f,  1.0f), ds::Vec2(2.0f,  3.0f), 1.0f, ds::Vec3(0.1f, 0.8f, 0.0f)),
		wall::Wall(ds::Vec2(2.0f,  3.0f), ds::Vec2(-3.0f,  3.0f), 1.0f, ds::Vec3(0.1f, 0.0f, 0.8f)),
		wall::Wall(ds::Vec2(-3.0f,  3.0f), ds::Vec2(-3.0f, -1.0f), 1.0f, ds::Vec3(0.8f, 0.0f, 0.1f)),
		wall::Wall(ds::Vec2(-3.0f, -1.0f), ds::Vec2(0.0f, -1.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall(ds::Vec2(0.0f, -1.0f), ds::Vec2(0.0f, -2.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall(ds::Vec2(0.0f, -2.0f), ds::Vec2(-3.0f, -2.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall(ds::Vec2(-3.0f, -2.0f), ds::Vec2(-3.0f, -4.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall(ds::Vec2(-3.0f, -4.0f), ds::Vec2(1.0f, -4.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall(ds::Vec2(2.0f, -4.0f), ds::Vec2(3.0f, -4.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall(ds::Vec2(3.0f, -4.0f), ds::Vec2(3.0f, -2.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall(ds::Vec2(3.0f, -2.0f), ds::Vec2(2.0f, -2.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall(ds::Vec2(2.0f, -2.0f), ds::Vec2(2.0f,  0.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
		wall::Wall(ds::Vec2(2.0f,  0.0f), ds::Vec2(4.0f,  0.0f), 1.0f, ds::Vec3(0.8f, 0.1f, 0.0f)),
	};

	std::vector<rendering::Sprite> sprites;

	std::vector<ds::Vec2> coinPositions =
	{
		{2.4f, 1.9f},
		{0.1f, 1.5f},
		{1.5f, 4.0f}
	};

	std::vector<ds::Vec2> treePositions =
	{
		{100.0f, 10.0f},
		{17.0f, 12.0f},
		{-10.0f, -8.0f}
	};

	for (const auto& pos : coinPositions)
	{
		auto coin = rendering::spriteFromTexture(textures[2]);
		coin.size = 0.3f;
		coin.position = pos;
		coin.height = -0.2f;

		sprites.push_back(coin);
	}

	for (const auto& pos : treePositions)
	{
		auto tree = rendering::spriteFromTexture(textures[3]);

		tree.position = pos;
		tree.size = 2.0f;
		tree.height = -2.0f;

		sprites.push_back(tree);
	}

	camera::Camera camera = camera::Camera();

	auto timeBefore = std::chrono::high_resolution_clock::now();
	auto timeNow = std::chrono::high_resolution_clock::now();
	float cumulativeTime = 0.0f;
	int numFrames = 0;

	while (!eventHandler.shouldQuit())
	{
		timeNow = std::chrono::high_resolution_clock::now();
		const float deltaTimeSec = std::chrono::duration_cast<std::chrono::milliseconds>(timeNow - timeBefore).count() / 1000.0f;
		timeBefore = timeNow;

		processInput(eventHandler, camera, mainContext, deltaTimeSec);

		camera::updateCamera(camera);

		renderMain(mainContext, camera, walls, textures, sprites);

		numFrames += 1;
		cumulativeTime += deltaTimeSec;

		if (cumulativeTime > 1.0f)
		{
			std::cout << "Frame: " << numFrames / cumulativeTime << std::endl;
			cumulativeTime = 0.0f;
			numFrames = 0;
		}
	}

	return 0;
}

