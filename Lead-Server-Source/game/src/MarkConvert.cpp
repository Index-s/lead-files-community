#include "stdafx.h"
#include "MarkManager.h"

#ifdef __WIN32__
#include <direct.h>
#endif

#define OLD_MARK_INDEX_FILENAME "guild_mark.idx"
#define OLD_MARK_DATA_FILENAME "guild_mark.tga"

static Pixel * LoadOldGuildMarkImageFile()
{
	FILE * fp = fopen(OLD_MARK_DATA_FILENAME, "rb");

	if (!fp)
	{
		sys_err("cannot open %s", OLD_MARK_INDEX_FILENAME);
		return NULL;
	}

	int dataSize = 512 * 512 * sizeof(Pixel);
	Pixel * dataPtr = (Pixel *) malloc(dataSize);

	fread(dataPtr, dataSize, 1, fp);

	fclose(fp);

	return dataPtr;
}

bool GuildMarkConvert(const std::vector<DWORD> & vecGuildID)
{
	// Create folder
#ifndef __WIN32__
	mkdir("mark", S_IRWXU);
#else
	_mkdir("mark");
#endif

	// Is there an index file? ? 
#ifndef __WIN32__
	if (0 != access(OLD_MARK_INDEX_FILENAME, F_OK))
#else
	if (0 != _access(OLD_MARK_INDEX_FILENAME, 0))
#endif
		return true;

	// Open index file
	FILE* fp = fopen(OLD_MARK_INDEX_FILENAME, "r");

	if (NULL == fp)
		return false;

	// Open image file
	Pixel * oldImagePtr = LoadOldGuildMarkImageFile();

	if (NULL == oldImagePtr)
	{
		fclose(fp);
		return false;
	}

	/*
	// guild_mark.tga is real targa not a file , 512 * 512 * 4 of size raw It's a file .
	// In order to see it with your own eyes, targa make it into a file .
	CGuildMarkImage * pkImage = new CGuildMarkImage;
	pkImage->Build("guild_mark_real.tga");
	pkImage->Load("guild_mark_real.tga");
	pkImage->PutData(0, 0, 512, 512, oldImagePtr);
	pkImage->Save("guild_mark_real.tga");
	*/
	sys_log(0, "Guild Mark Converting Start.");

	char line[256];
	DWORD guild_id;
	DWORD mark_id;
	Pixel mark[SGuildMark::SIZE];

	while (fgets(line, sizeof(line)-1, fp))
	{
#ifdef _WIN32
		sscanf_s(line, "%u %u", &guild_id, &mark_id);
#else
		sscanf(line, "%u %u", &guild_id, &mark_id);
#endif

		if (find(vecGuildID.begin(), vecGuildID.end(), guild_id) == vecGuildID.end())
		{
			sys_log(0, "  skipping guild ID %u", guild_id);
			continue;
		}

		// mark id -> Find location in image
		uint row = mark_id / 32;
		uint col = mark_id % 32;

		if (row >= 42)
		{
			sys_err("invalid mark_id %u", mark_id);
			continue;
		}

		uint sx = col * 16;
		uint sy = row * 12;

		Pixel * src = oldImagePtr + sy * 512 + sx;
		Pixel * dst = mark;

		// Copy one mark from an old image
		for (int y = 0; y != SGuildMark::HEIGHT; ++y)
		{
			for (int x = 0; x != SGuildMark::WIDTH; ++x)
				*(dst++) = *(src+x);

			src += 512;
		}

		// Added to the new guild mark system .
		CGuildMarkManager::instance().SaveMark(guild_id, (BYTE *) mark);
		line[0] = '\0';
	}

	free(oldImagePtr);
	fclose(fp);

	// Conversion only needs to be done once, so move the file. .
#ifndef __WIN32__
	system("mv -f guild_mark.idx guild_mark.idx.removable");
	system("mv -f guild_mark.tga guild_mark.tga.removable");
#else
	system("move /Y guild_mark.idx guild_mark.idx.removable");
	system("move /Y guild_mark.tga guild_mark.tga.removable");
#endif

	sys_log(0, "Guild Mark Converting Complete.");

	return true;
}

