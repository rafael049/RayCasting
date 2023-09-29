#pragma once

#include <glm/vec4.hpp>

#include "sdl.hpp"


namespace rendering
{
	using ScreenBuffer = std::vector<uint32_t>;


	struct Context
	{
		SDL::SDLWindowPtr window;
		SDL::SDLRendererPtr renderer;
		SDL::SDLTexturePtr screenTexture;
		size_t width, height;
		ScreenBuffer screenBuffer;

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
		SDL::updateTexture(context.screenTexture, context.screenBuffer);

		SDL::renderCopy(context.renderer, context.screenTexture);

		SDL::renderPresent(context.renderer);
	}


	auto clearContext(Context& context)
	{
		std::memset(context.screenBuffer.data(), 0, context.screenBuffer.size() * sizeof(context.screenBuffer[0]));
	}
}
