#pragma once

#include <vector>
#include <fstream>
#include <string>
#include <expected>
#include <format>

#include <glm/vec4.hpp>

namespace media
{
	struct Image
	{
		std::vector<glm::vec4> data;

		size_t width;
		size_t height;

		Image(size_t width, size_t height) :
			data(width* height), height(height), width(width)
		{
			
		}

		Image(const Image& image)
		{
			data = image.data;

			width = image.width;
			height = image.height;
		}
	};


	auto imageFromBitMapFile(const std::string& filename) -> std::expected<Image, std::string>
	{
		std::ifstream fileStream(filename, std::ios::binary);

		if (fileStream.is_open())
		{

			// Check if is a BMP file

			char magicNumber[2] = {0, 0};

			fileStream.read(magicNumber, 2);

			if (magicNumber[0] != 'B' || magicNumber[1] != 'M')
			{
				return std::unexpected(std::format("File '{}' is not a BMP file, invalid magic number", filename));
			}

			// Get pixel array location

			fileStream.seekg(0x0A, std::ios::beg);

			uint32_t pixelsArrayOffset = 0;

			fileStream.read(reinterpret_cast<char*>(&pixelsArrayOffset), sizeof(pixelsArrayOffset));

			if (pixelsArrayOffset == 0)
			{
				return std::unexpected(std::format("Failed to get pixels locations from BMP image '{}'", filename));
			}

			// Verify DIB header

			uint32_t dibHeaderSize = 0;

			fileStream.seekg(0x0E, std::ios::beg);
			fileStream.read(reinterpret_cast<char*>(&dibHeaderSize), sizeof(dibHeaderSize));

			if (dibHeaderSize != 40 && dibHeaderSize != 124)
			{
				return std::unexpected(std::format("Invalid DIB header for file '{}'", filename));
			}


			// Get width and height

			int32_t width = 0;
			int32_t height = 0;

			fileStream.seekg(0x12, std::ios::beg);
			fileStream.read(reinterpret_cast<char*>(&width), sizeof(width));
			fileStream.seekg(0x16, std::ios::beg);
			fileStream.read(reinterpret_cast<char*>(&height), sizeof(height));



			// Get color depth

			uint32_t colorDepth = 0;

			fileStream.seekg(0x1C, std::ios::beg);
			fileStream.read(reinterpret_cast<char*>(&colorDepth), sizeof(colorDepth));

			if (colorDepth != 24)
			{
				return std::unexpected(std::format("Not supported color depth for BMP '{}'.", filename));
			}

			// Get pixel Array

			size_t rowSize = (size_t)std::ceil(colorDepth * width / 32) * 4;

			std::vector<uint8_t> buffer(rowSize * height);

			fileStream.seekg(pixelsArrayOffset, std::ios::beg);
			fileStream.read(reinterpret_cast<char*>(buffer.data()), buffer.size());

			// Fill image pixel data
			Image image = Image(width, height);

			size_t imageIndex = 0;

			for (size_t i = 0; i < height; i++)
			{
				for (size_t j = 0; j < rowSize; j += 3)
				{
					size_t rowOffset = i * rowSize;

					auto color = glm::vec4(
						buffer[rowOffset + j + 2] / 255.0f, 
						buffer[rowOffset + j + 1] / 255.0f, 
						buffer[rowOffset + j] / 255.0f, 
						1.0f);

					image.data[imageIndex] = color;

					imageIndex += 1;
				}
			}

			return image;
		}
		else
		{
			return std::unexpected(std::format("Failed to open file '{}'", filename));
		}
	}
}