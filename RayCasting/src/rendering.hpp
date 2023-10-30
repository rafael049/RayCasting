#pragma once

#include <glm/vec4.hpp>

#include "sdl.hpp"


namespace rendering
{
	template <typename T>
	using ScreenBuffer = std::vector<T>;

	struct Texture
	{
		std::vector<media::Image> mipmaps;

		size_t width;
		size_t height;
	};

	
	struct Sprite
	{
		Texture texture;

		ds::Vec2 position;
		float size;
		float height;
	};


	auto spriteFromTexture(rendering::Texture& texture) -> rendering::Sprite
	{
		return rendering::Sprite{ rendering::Texture{texture.mipmaps, texture.width, texture.height}, ds::Vec2(0.0f, 0.0f), 1.0f, 0.0f};
	}


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
					image.data[(_i + 1) * image.width + _j + 1])/4;
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
		ScreenBuffer<uint32_t> screenBuffer;
		ScreenBuffer<uint8_t> stencilBuffer;
		ScreenBuffer<float> depthBuffer;

		bool useMipmap = true;
		bool useFiltering = true;

		Context(SDL::SDLWindowPtr&& window, SDL::SDLRendererPtr&& renderer, size_t width, size_t height) :
			window (std::move(window)),
			renderer(std::move(renderer)),
			screenBuffer(width * height),
			stencilBuffer(width * height),
			depthBuffer(width * height, 1.0f),
			width(width),
			height(height)
		{
			screenTexture = *SDL::createTexture(this->renderer, width, height);
		}
	};


	auto setSceenBufferPixel(Context& context, const size_t x, const size_t y, const glm::u8vec4& color)
	{
		std::array<uint8_t, 4> colorDWord = 
		{
			(uint8_t)(color.a),
			(uint8_t)(color.b),
			(uint8_t)(color.g),
			(uint8_t)(color.r),
		};

		context.screenBuffer[y * context.width + x] = *((uint32_t*)colorDWord.data());
	}


	auto setStencilBufferPixel(Context& context, const size_t x, const size_t y, uint8_t value)
	{
		context.stencilBuffer[y * context.width + x] = value;
	}


	auto setDepthBufferPixel(Context& context, const size_t x, const size_t y, float value)
	{
		context.depthBuffer[y * context.width + x] = value;
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
		std::memset(context.stencilBuffer.data(), 0, context.stencilBuffer.size() * sizeof(context.stencilBuffer[0]));

		for (size_t i = 0; i < context.depthBuffer.size(); i++)
		{
			context.depthBuffer[i] = 1.0f;
		}
	}
}
