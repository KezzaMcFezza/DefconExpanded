#include "rar.hpp"
#include "unrar.h"
#include <cstring>
#include <cstdlib>

//*****************************************************************************
// Class MemMappedFile
//*****************************************************************************

MemMappedFile::MemMappedFile(char const *_filename, unsigned int _size)
:	m_size(_size)
{
	m_data = new unsigned char[_size];
	m_filename = strdup(_filename);
	unsigned int len = strlen(_filename);
	for (unsigned int i = 0; i < len; ++i)
	{
		if (m_filename[i] == '\\') 
		{
			m_filename[i] = '/';
		}
	}
}

MemMappedFile::~MemMappedFile()
{
	m_size = 0;
	delete[] m_data;
	free(m_filename);
}

//*****************************************************************************
// Class UncompressedArchive
//*****************************************************************************

UncompressedArchive::UncompressedArchive(char const *_filename, char const *_password)
:	m_numFiles(0)
{
	ErrHandler.SetSignalHandlers(true);
	ErrHandler.SetSilent(true);
	
#ifdef _WIN_ALL
	SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#endif

	CommandData *Cmd = new CommandData();
	Cmd->Init();
	
	Cmd->Command = L"e";
	
	Cmd->Overwrite = OVERWRITE_ALL;
	Cmd->ExclPath = EXCL_SKIPWHOLEPATH;
	Cmd->Test = false;
	Cmd->DisablePercentage = true;
	Cmd->DisableCopyright = true;
	Cmd->DisableDone = true;
	Cmd->MsgStream = MSG_NULL;
	Cmd->Sound = SOUND_NOTIFY_OFF;  

	if (_password && _password[0] != '\0')
	{
		std::wstring wpassword;
		CharToWide(_password, wpassword);
		Cmd->Password.Set(wpassword.c_str());
	}
	
	std::wstring wfilename;
	CharToWide(_filename, wfilename);
	
	Archive Arc(Cmd);
	
	if (!Arc.WOpen(wfilename))
	{
		delete Cmd;
		return;
	}
	
	if (!Arc.IsArchive(true))
	{
		Arc.Close();
		delete Cmd;
		return;
	}
	
	ComprDataIO DataIO;
	DataIO.Init();
	DataIO.SetCurrentCommand('e');
	DataIO.SetTestMode(false);
	DataIO.SetSkipUnpCRC(false);
	DataIO.SetFiles(&Arc, NULL);
	DataIO.SetPackedSizeToRead(0);
	
	Unpack Unp(&DataIO);
	size_t HeaderSize = 0;
	
	while ((HeaderSize = Arc.ReadHeader()) > 0)
	{
		HEADER_TYPE HeaderType = Arc.GetHeaderType();
		
		if (HeaderType == HEAD_FILE)
		{
			char filename[1024];
			WideToChar(Arc.FileHead.FileName.c_str(), filename, sizeof(filename));
			
			unsigned int unpacked_size = (unsigned int)Arc.FileHead.UnpSize;
			
			MemMappedFile *mmf = AllocNewFile(filename, unpacked_size);
			
			if (mmf != NULL && unpacked_size > 0)
			{
				DataIO.SetUnpackToMemory(mmf->m_data, mmf->m_size);
				
				Unp.Init(Arc.FileHead.WinSize, Arc.FileHead.Solid);
				Unp.SetDestSize(Arc.FileHead.UnpSize);
				
				DataIO.SetPackedSizeToRead(Arc.FileHead.PackSize);
				DataIO.UnpVolume = Arc.FileHead.SplitAfter;
				DataIO.SetSubHeader(&Arc.FileHead, NULL);
				
				if (Arc.FileHead.Encrypted)
				{
					if (Cmd->Password.IsSet())
					{
						DataIO.SetEncryption(
							false,
							Arc.FileHead.CryptMethod,
							&Cmd->Password,
							Arc.FileHead.SaltSet ? Arc.FileHead.Salt : NULL,
							Arc.FileHead.InitV,
							Arc.FileHead.Lg2Count,
							Arc.FileHead.HashKey,
							Arc.FileHead.PswCheck
						);
					}
					else
					{
						Arc.SeekToNext();
						continue;
					}
				}
				
				DataIO.UnpHash.Init(Arc.FileHead.FileHash.Type, 1);
				
				if (Arc.FileHead.Method == 0)
				{
					CmdExtract::UnstoreFile(DataIO, Arc.FileHead.UnpSize);
				}
				else
				{
					Unp.DoUnpack(Arc.FileHead.UnpVer, Arc.FileHead.Solid);
				}
				
				if (!DataIO.UnpHash.Cmp(&Arc.FileHead.FileHash, 
					Arc.FileHead.UseHashKey ? Arc.FileHead.HashKey : NULL))
				{
				}
				
				DataIO.SetUnpackToMemory(NULL, 0);
			}
			else
			{
				Arc.SeekToNext();
			}
		}
		else if (HeaderType == HEAD_ENDARC)
		{
			break;
		}
		else
		{
			Arc.SeekToNext();
		}
	}
	
	Arc.Close();
	delete Cmd;
}

UncompressedArchive::~UncompressedArchive()
{
	for (unsigned int i = 0; i < m_numFiles; ++i)
	{
		if (m_files[i] != NULL)
		{
			delete m_files[i];
			m_files[i] = NULL;
		}
	}
	m_numFiles = 0;
}

MemMappedFile *UncompressedArchive::AllocNewFile(char const *_filename, unsigned int _size)
{
	if (m_numFiles >= MAX_FILES)
	{
		return NULL;
	}

	m_files[m_numFiles] = new MemMappedFile(_filename, _size);
	m_numFiles++;

	return m_files[m_numFiles - 1];
}