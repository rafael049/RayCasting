#pragma once

#include <glm/vec4.hpp>

#include "sdl.hpp"


namespace rendering
{
	using ScreenBuffer = std::vector<uint32_t>;

	struct Texture
	{
		std::vector<media::Image> mipmaps;

		size_t width;
		size_t height;
	};


	auto minifyImage(const media::Image& image) -> media::Image
	{
		auto result = media::Image(image.width / 2, image.height / 2);

		for (int i = 0; i < result.height; i++)
		{
			for (int j = 0; j < result.width; j++)
			{
				int _i = 2 * i;
				int _j = 2 * j;
				result.data[i*result.width + j] = (
					image.data[_i       * image.width + _j    ] +
					image.data[_i       * image.width + _j + 1] +
					image.data[(_i + 1) * image.width + _j    ] +
					image.data[(_i + 1) * image.width + _j + 1])/4.0f;
			}
		}

		return result;
	}


	auto createTexture(const media::Image&& image) -> Texture
	{
		media::Image m1 = image;
		media::Image m2 = std::move(minifyImage(m1));
		media::Image m3 = std::move(minifyImage(m2));
		media::Image m4 = std::move(minifyImage(m3));

		return Texture{ {m1, m2, m3, m4}, m1.width, m1.height };
	}


	struct Context
	{
		SDL::SDLWindowPtr window;
		SDL::SDLRendererPtr renderer;
		SDL::SDLTexturePtr screenTexture;
		size_t width, height;
		ScreenBuffer screenBuffer;

		bool useMipmap = true;

		Context(SDL::SDLWindowPtr&& window, SDL::SDLRendererPtr&& renderer, size_t width, size_t height) :
			window (std::move(window)),
			renderer(std::move(renderer)),
			screenBuffer(width * height),
			width(width),
			height(height)
		{
			screenTexture = *SDL::createTexture(this->renderer, width, height);
		}
	};


	auto setSceenBufferPixel(Context& context, const size_t x, const size_t y, const glm::vec4& color)
	{
		std::array<uint8_t, 4> colorDWord = 
		{
			(uint8_t)(255 * color.a),
			(uint8_t)(255 * color.b),
			(uint8_t)(255 * color.g),
			(uint8_t)(255 * color.r),
		};

		context.screenBuffer[y * context.width + x] = *((uint32_t*)colorDWord.data());
	}


	auto renderContext(Context& context)
	{
		SDL::updateTexture(context.screenTexture, context.screenBuffer, context.width);

		SDL::renderCopy(context.renderer, context.screenTexture);

		SDL::renderPresent(context.renderer);
	}


	auto clearContext(Context& context)
	{
		std::memset(context.screenBuffer.data(), 0, context.screenBuffer.size() * sizeof(context.screenBuffer[0]));
	}
}
