#pragma once

struct Sound
{
	short* samples;
	size_t count;
	size_t pos;
	bool32 loop;
};