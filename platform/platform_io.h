#ifndef TCE_PLATFORM_IO_H
#define TCE_PLATFORM_IO_H

#include <stddef.h>
#include <string.h>
#ifdef _WIN32
#include <stdint.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

/* Class A: separator for paths consumed by native OS file APIs only.
 * Engine virtual-filesystem paths keep '/' on every platform. Mod gameplay
 * continues to use its engine filesystem traps; this header does not create
 * a second filesystem or replace SDK PATH_SEP. */
#if defined(_WIN32)
#define TCE_PLATFORM_NATIVE_PATH_SEPARATOR '\\'
#else
#define TCE_PLATFORM_NATIVE_PATH_SEPARATOR '/'
#endif

typedef void (*tcePlatformDirectoryVisitor_t)(void *context, const char *name,
                                            int directory);

/* Class A: native directory enumeration for server-owned loose Lua files.
 * The caller supplies bounded scratch space. Entries keep native order;
 * filtering, deduplication, root selection and byte limits belong to the
 * caller. POSIX stat follows links, as before; Windows uses _A_SUBDIR.
 * The callback borrows the entry name only until its return. This is not
 * an engine VFS listing and intentionally does not recurse. */
static __inline int TCE_PlatformListDirectory(const char *directory, char *scratch,
                                             size_t capacity,
                                             tcePlatformDirectoryVisitor_t visit,
                                             void *context)
{
	size_t length;
	if (!directory || !scratch || !visit)
	{
		return 0;
	}
	length = strlen(directory);
#ifdef _WIN32
	{
		struct _finddata_t entry;
		intptr_t search;
		if (length > capacity || capacity - length < 3)
		{
			return 0;
		}
		memcpy(scratch, directory, length);
		memcpy(scratch + length, "/*", 3);
		search = _findfirst(scratch, &entry);
		if (search == -1)
		{
			return 0;
		}
		do
		{
			visit(context, entry.name, (entry.attrib & _A_SUBDIR) != 0);
		}
		while (!_findnext(search, &entry));
		_findclose(search);
	}
#else
	{
		DIR *dir = opendir(directory);
		struct dirent *entry;
		struct stat st;
		if (!dir)
		{
			return 0;
		}
		while ((entry = readdir(dir)) != NULL)
		{
			size_t nameLength = strlen(entry->d_name);
			if (length > capacity || capacity - length < 2 ||
			    nameLength > capacity - length - 2)
			{
				continue;
			}
			memcpy(scratch, directory, length);
			scratch[length] = '/';
			memcpy(scratch + length + 1, entry->d_name, nameLength + 1);
			if (!stat(scratch, &st))
			{
				visit(context, entry->d_name, S_ISDIR(st.st_mode));
			}
		}
		closedir(dir);
	}
#endif
	return 1;
}

#endif
