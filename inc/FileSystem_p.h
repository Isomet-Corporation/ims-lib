/*-----------------------------------------------------------------------------
/ Title      : Synthesiser File System Private Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/FileSystem/h/FileSystem_p.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2017-09-11 23:55:34 +0100 (Mon, 11 Sep 2017) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 300 $
/------------------------------------------------------------------------------
/ Description:
/------------------------------------------------------------------------------
/ Copyright (c) 2015 Isomet (UK) Ltd. All Rights Reserved.
/------------------------------------------------------------------------------
/ Revisions  :
/ Date        Version  Author  Description
/ 2015-04-09  1.0      dc      Created
/
/----------------------------------------------------------------------------*/

#ifndef IMS_FILESYSTEM_P_H__
#define IMS_FILESYSTEM_P_H__

#include "FileSystem.h"

namespace iMS {

	const int FileSystemTableStartAddress = 512;
	const int FileSystemTableLength = 512;
	const int FileSystemStartAddress = FileSystemTableStartAddress + FileSystemTableLength;
	const int FileSystemEndAddress = 131071;

	const std::uint8_t FILESYSTEM_VER = 1;

	/* FILE SYSTEM TABLE INTERNAL */
	class FileSystemTable
	{
	public:
		using FSTEArray = std::array < FileSystemTableEntry, MAX_FST_ENTRIES > ;

		FileSystemTable();
		~FileSystemTable();
		FileSystemTable(const FileSystemTable& other);
		FileSystemTable& operator= (const FileSystemTable& other);

		bool Initialise(std::vector<std::uint8_t> datastream);
		bool IsValid() const { return valid; };
		const std::uint8_t& Entries() const { return nEntries; };
		std::uint8_t& Entries() { return nEntries; }
		FileSystemTableEntry& operator[](std::size_t idx);
		const FileSystemTableEntry& operator[](std::size_t idx) const;
		bool CheckNewEntry(const FileSystemTableEntry&) const;
		FileSystemIndex NextFreeEntry() const;
		bool FindFreeSpace(std::uint32_t& Addr, std::uint32_t Size) const;
		void UpdateEntryCount();
		FileSystemIndex GetIndexFromName(const std::string& name) const;
	private:
		bool valid;
		std::uint8_t nEntries;
		std::uint8_t version;
		FSTEArray *m_FSTEArray;

		//FileSystemEventTrigger m_Event;
	};

	class FileSystemTableReader
	{
	public:
		FileSystemTableReader(const IMSSystem& ims);
		~FileSystemTableReader();
		FileSystemTable Readback();
	private:
		// Make this object non-copyable
		FileSystemTableReader(const FileSystemTableReader &);
		const FileSystemTableReader &operator =(const FileSystemTableReader &);

		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};

	class FileSystemTableWriter
	{
	public:
		FileSystemTableWriter(IMSSystem& ims, const FileSystemTable&);
		~FileSystemTableWriter();
		bool Program();
	private:
		// Make this object non-copyable
		FileSystemTableWriter(const FileSystemTableWriter &);
		const FileSystemTableWriter &operator =(const FileSystemTableWriter &);

		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};

	class FileSystemReader
	{
	public:
		FileSystemReader(const IMSSystem& ims, const FileSystemIndex index);
		FileSystemReader(const IMSSystem& ims, const std::string FileName);
		~FileSystemReader();
		bool Readback(std::vector<std::uint8_t>&);
	private:
		// Make this object non-copyable
		FileSystemReader(const FileSystemReader &);
		const FileSystemReader &operator =(const FileSystemReader &);

		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};

	class FileSystemWriter
	{
	public:
		FileSystemWriter(IMSSystem& ims, const FileSystemTableEntry& fste, const std::vector<std::uint8_t>& file_data);
		~FileSystemWriter();
		FileSystemIndex Program();
	private:
		// Make this object non-copyable
		FileSystemWriter(const FileSystemWriter &);
		const FileSystemWriter &operator =(const FileSystemWriter &);

		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};


}
#endif
