/*
 *  ZXTranslator.cpp 2.0.1 (May 2nd, 1998)
 *
 *  Translator add-on for ZX Spectrum SCREEN$ pictures
 *  Written by Edmund Vermeulen <edmundv@xs4all.nl>
 *  Public domain.
 */


// Includes
#include <string.h>
#include <TranslatorAddOn.h>
#include <TranslatorFormats.h>
#include <DataIO.h>
#include <Screen.h>


// Defines
#define ZX_FORMAT 'ZX  '


// Translator info
char translatorName[] = "ZXTranslator";
char translatorInfo[] = "By Edmund Vermeulen <edmundv@xs4all.nl>";
int32 translatorVersion = 201;

translation_format inputFormats[] =
{
	{ ZX_FORMAT, B_TRANSLATOR_BITMAP, 0.9, 0.8, "image/x-zx", "ZX Spectrum SCREEN$" },
	{ 0, 0, 0.0, 0.0, "", "" }
};

translation_format outputFormats[] =
{
	{ B_TRANSLATOR_BITMAP, B_TRANSLATOR_BITMAP, 0.3, 0.5, "image/x-be-bitmap", "Be bitmap" },
	{ 0, 0, 0.0, 0.0, "", "" }
};


status_t
Identify(
	BPositionIO *inSource,
	const translation_format *inFormat,
	BMessage * /*ioExtension*/,
	translator_info *outInfo,
	uint32 outType)
{
	if (outType && (outType != B_TRANSLATOR_BITMAP))
		return B_NO_TRANSLATOR;

	uchar *file_buffer = new uchar[7397];
	ssize_t file_length = inSource->Read(file_buffer, 7397);
	delete[] file_buffer;

	// Recognise a ZX SCREEN$ by its file length
	if (file_length == 6912 ||
	    file_length == 6921 ||
	    file_length == 6924 ||
	    file_length == 7396)
	{
		inFormat = &inputFormats[0];
		outInfo->type = inFormat->type;
		outInfo->group = inFormat->group;
		outInfo->quality = inFormat->quality;
		outInfo->capability = inFormat->capability;
		strcpy(outInfo->name, inFormat->name);
		strcpy(outInfo->MIME, inFormat->MIME);
	
		return B_OK;
	}

	return B_NO_TRANSLATOR;
}


status_t
Translate(
	BPositionIO *inSource,
	const translator_info *inInfo,
	BMessage * /*ioExtension*/,
	uint32 outFormat,
	BPositionIO *outDestination)
{
	status_t ret_val = B_OK;

	//	Check that we handle input and output types
	if (inInfo->type != ZX_FORMAT)
		return B_NO_TRANSLATOR;

	if (outFormat == 0)
		outFormat = B_TRANSLATOR_BITMAP;

	if (outFormat != B_TRANSLATOR_BITMAP)
		return B_NO_TRANSLATOR;

	// Fill in the bitmap header
	TranslatorBitmap bmp; 
	bmp.magic = B_TRANSLATOR_BITMAP;
	bmp.bounds = BRect(0.0, 0.0, 255.0, 191.0);
	bmp.rowBytes = 256;
	bmp.colors = B_CMAP8;
	bmp.dataSize = 256 * 192;

	// Create the spectrum colour palette
	uchar zx_col[16];
	BScreen *the_screen = new BScreen;
	for (int32 n = 0; n < 16; ++n)
	{
		uchar red   = n & 2 ? (n & 8 ? 0xFFFFFFFF : 0xAAAAAAAA) : 0x00;
		uchar green = n & 4 ? (n & 8 ? 0xFFFFFFFF : 0xAAAAAAAA) : 0x00;
		uchar blue  = n & 1 ? (n & 8 ? 0xFFFFFFFF : 0xAAAAAAAA) : 0x00;

		zx_col[n] = the_screen->IndexForColor(red, green, blue);
	}
	delete the_screen;

	uchar *file_buffer = new uchar[7397];

	ssize_t file_length = inSource->Read(file_buffer, 7397);
	if (file_length == 6912 ||
	    file_length == 6921 ||
	    file_length == 6924 ||
	    file_length == 7396)
	{
		uchar *pixel;
		if (file_length == 7396)
			pixel = file_buffer + 265;
		else
			pixel = file_buffer + file_length - 6912;

		// Decode the attribute area
		uchar (* ink)[24] = new uchar[32][24];
		uchar (* paper)[24] = new uchar[32][24];

		uchar *attr = pixel + 6144;
		for (int y = 0; y < 24; ++y)
		{
			for (int x = 0; x < 32; ++x)
			{
				paper[x][y] = (*attr & 56) >> 3;
				ink[x][y] = *attr & 7;

				// Bright?
				if (*attr & 64)
				{
					ink[x][y] += 8;
					paper[x][y] += 8;
				}

				// Bright black = normal black
				if (paper[x][y] == 0)
					paper[x][y] = 8;
				if (ink[x][y] == 0)
					ink[x][y] = 8;

				++attr;
			}
		}

		uchar *zx_bitmap = new uchar[256 * 192]; 

		// Write the bitmap
		for (int l = 0; l < 3; ++l)
		{
			for (int k = 0; k < 8; ++k)
			{
				for (int j = 0; j < 8; ++j)
				{
					int y = l * 64 + k + j * 8;

					for (int i = 0; i < 32; ++i)
					{
						uchar this_ink = zx_col[ink[i][y >> 3]];
						uchar this_paper = zx_col[paper[i][y >> 3]];

						int mask = 1 << 7;
						for (int h = 0; h < 8; ++h)
						{
							if (*pixel & mask)
								zx_bitmap[y * 256 + (i << 3) + h] = this_ink;
							else
								zx_bitmap[y * 256 + (i << 3) + h] = this_paper;

							mask >>= 1;
						}
						++pixel;
					}
				}
			}
		}

		// Convert the header to the correct endianess
		swap_data(B_UINT32_TYPE, &(bmp.magic), sizeof(uint32), B_SWAP_BENDIAN_TO_HOST);
		swap_data(B_RECT_TYPE, &(bmp.bounds), sizeof(BRect), B_SWAP_BENDIAN_TO_HOST);
		swap_data(B_UINT32_TYPE, &(bmp.rowBytes), sizeof(uint32), B_SWAP_BENDIAN_TO_HOST);
		swap_data(B_UINT32_TYPE, &(bmp.colors), sizeof(color_space), B_SWAP_BENDIAN_TO_HOST);
		swap_data(B_UINT32_TYPE, &(bmp.dataSize), sizeof(uint32), B_SWAP_BENDIAN_TO_HOST);

		// Write out bitmap header & bitmap data
		if (outDestination->Write(&bmp, sizeof(TranslatorBitmap)) != sizeof(TranslatorBitmap) ||
		    outDestination->Write(zx_bitmap, 256 * 192) != 256 * 192)
		{
			ret_val = B_ERROR;
		}

		delete[] zx_bitmap;
		delete[] paper;
		delete[] ink;
	}
	else
	{
		ret_val = B_NO_TRANSLATOR;
	}

	delete[] file_buffer;
	return ret_val;
}
