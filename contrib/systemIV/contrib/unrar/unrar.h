#ifndef INCLUDED_UNRAR_H
#define INCLUDED_UNRAR_H

#define MAX_FILES	16384

class MemMappedFile
{
public:
	char			*m_filename;
	unsigned char	*m_data;
	unsigned int	m_size;

	MemMappedFile(char const *_filename, unsigned int _size);
	~MemMappedFile();
};

class UncompressedArchive
{
public:
	MemMappedFile	*m_files[MAX_FILES];
	unsigned int	m_numFiles;

	UncompressedArchive(char const *_filename, char const *_password);
	~UncompressedArchive();

	MemMappedFile *AllocNewFile(char const *_filename, unsigned int _size);

private:
	UncompressedArchive(const UncompressedArchive&);
	UncompressedArchive& operator=(const UncompressedArchive&);
};

#endif // INCLUDED_UNRAR_H