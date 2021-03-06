#ifndef FILEMANAGER_H
#define	FILEMANAGER_H

#include <stdio.h>

#define	MAX_QPATH		64			// max length of a quake game pathname
#define	MAX_OSPATH		128			// max length of a filesystem pathname

// in memory

typedef struct {
	char name[MAX_QPATH];
	int filepos, filelen;
} packfile_t;

typedef struct pack_s {
	char filename[MAX_OSPATH];
	int handle;
	int numfiles;
	packfile_t *files;
} pack_t;

typedef struct searchpath_s {
	char filename[MAX_OSPATH];
	pack_t *pack; // only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

class File {
private:
	FILE *obj;
	char *path;
	char openType[4];
public:
	enum FileOpenType { READ, WRITE, APPEND };
	
	File(const char *path, FileOpenType mode, bool isBinary);
	~File();
	
	bool isOpen();
	long getLength();
	size_t read(void *buffer, size_t size);
	size_t write(const void *buffer, size_t size);
	void seek(long position);
};

class FileManager {
public:
	static searchpath_t *searchpaths;

	/**
	 * Returns true if the filename passed in points to an existing file.
     */
	static bool FileExists(const char *filename);
	
	static int OpenFile(const char *filename, int *hndl);
	static int FOpenFile(const char *filename, FILE **file);
	static int FindFile(const char *filename, int *handle, FILE **file);
	static void WriteFile(const char *filename, void *data, int len);
	static void CloseFile(int h);
	/**
	 * Converts any illegal file characeters into legal ones.
	 *
	 * @param data to check and fix
	 */
	static void MakeFilenameValid(char *data);
	/**
	 * Copy string without extension.
     */
	static void StripExtension(const char *in, char *out);
	/**
	 * Return a pointer to the start of the file extension.
     */
	static const char *FileExtension(const char *in);
	/**
	 * If the path doesn't contain an extension .EXT then add the provided
	 * default.
	 */
	static void DefaultExtension(char *path, const char *extension);

	static void FileBase(const char *in, char *out);
	/**
	 * Console command to print out the search paths
     */
	static void PathCmd(void);
	/**
	 * Sets com_gamedir, adds the directory to the head of the path,
     * then loads and adds pak1.pak pak2.pak ...
     */
    static void AddGameDirectory(char *dir);
	/**
	 * Add a pak to our list of places to search for files
     */
	static void AddPackToPath(const char *pak);
	/**
     * Takes an explicit (not game tree related) path to a pak file.
     *
     * Loads the header and directory, adding the files at the beginning of the list
     * so they override previous pack files.
     */
    static pack_t *LoadPackFile(const char *packfile);
};

class SystemFileManager {
public:
	static int FileLength(FILE *f);
	static int FileOpenRead(const char *path, int *hndl);
	static int FileOpenWrite(const char *path);
	static void FileClose(int handle);
	static void FileSeek(int handle, long pos);
	static int FileRead(int handle, void *dest, size_t count);
	static size_t FileWrite(int handle, const void *data, size_t count);
private:
	static const int MAX_HANDLES = 10;
	static File *handles[MAX_HANDLES];
	
	static int findHandle();
	static File *getFileForHandle(int handle);
};

#endif	/* FILEMANAGER_H */

