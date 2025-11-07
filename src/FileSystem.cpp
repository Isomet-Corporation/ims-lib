/*-----------------------------------------------------------------------------
/ Title      : Synthesiser Filesystem Implementation
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/FileSystem/src/FileSystem.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2022-02-23 14:10:12 +0000 (Wed, 23 Feb 2022) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 521 $
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

#include "FileSystem.h"
#include "FileSystem_p.h"
#include "IEventTrigger.h"
#include "EEPROM.h"
#include "IMSSystem.h"
#include "HostReport.h"
#include "DeviceReport.h"
#include "IConnectionManager.h"
#include "PrivateUtil.h"

#include <iostream>
#include <iomanip>
#include <thread>
#include <cctype>

namespace iMS
{

	// Generic class used for iterating through FileSystemTypes
	template< typename T >
	class Enum
	{
	public:
		class Iterator
		{
		public:
			Iterator(int value) :
				m_value(value)
			{ }

			T operator*(void) const
			{
				return (T)m_value;
			}

			void operator++(void)
			{
				++m_value;
			}

			bool operator!=(Iterator rhs)
			{
				return m_value != rhs.m_value;
			}

		private:
			int m_value;
		};

	};

	static const FileSystemTypes FIRST = FileSystemTypes::NO_FILE;
	static const FileSystemTypes LAST = FileSystemTypes::USER_DATA;

	template< typename T >
	typename Enum<T>::Iterator begin(Enum<T>)
	{
		return typename Enum<T>::Iterator((int)FIRST);
	}

	template< typename T >
	typename Enum<T>::Iterator end(Enum<T>)
	{
		return typename Enum<T>::Iterator(((int)LAST) + 1);
	}


	/* FILE SYSTEM TABLE ENTRY */
	class FileSystemTableEntry::Impl {
	public:
		Impl() : m_type(FileSystemTypes::NO_FILE), m_address(0), m_length(0), m_def(FileDefault::NON_DEFAULT) {};
		Impl(FileSystemTypes type, std::uint32_t addr, std::uint32_t length, FileDefault def) :
			m_type(type), m_address(addr), m_length(length), m_def(def) {};
		Impl(FileSystemTypes type, std::uint32_t addr, std::uint32_t length, FileDefault def, std::string name) :
			m_type(type), m_address(addr), m_length(length), m_def(def), m_name(name) {};

		FileSystemTypes m_type;
		std::uint32_t m_address;
		std::uint32_t m_length;
		FileDefault m_def;
		std::string m_name;
	};

	FileSystemTableEntry::FileSystemTableEntry() : p_Impl(new Impl()) {}
		
	FileSystemTableEntry::FileSystemTableEntry(FileSystemTypes type, std::uint32_t addr, std::uint32_t length, FileDefault def) :
		p_Impl(new Impl(type, addr, length, def)) {}

	FileSystemTableEntry::FileSystemTableEntry(FileSystemTypes type, std::uint32_t addr, std::uint32_t length, FileDefault def, std::string name) :
		p_Impl(new Impl(type, addr, length, def, name)) {}

	FileSystemTableEntry::~FileSystemTableEntry() { delete p_Impl; p_Impl = nullptr; }

	FileSystemTableEntry::FileSystemTableEntry(const FileSystemTableEntry &rhs) : p_Impl(new Impl())
	{
		p_Impl->m_address = rhs.p_Impl->m_address;
		p_Impl->m_def = rhs.p_Impl->m_def;
		p_Impl->m_length = rhs.p_Impl->m_length;
		p_Impl->m_name = rhs.p_Impl->m_name;
		p_Impl->m_type = rhs.p_Impl->m_type;
	}

	FileSystemTableEntry &FileSystemTableEntry::operator =(const FileSystemTableEntry &rhs)
	{
		if (this == &rhs) return *this;
		p_Impl->m_address = rhs.p_Impl->m_address;
		p_Impl->m_def = rhs.p_Impl->m_def;
		p_Impl->m_length = rhs.p_Impl->m_length;
		p_Impl->m_name = rhs.p_Impl->m_name;
		p_Impl->m_type = rhs.p_Impl->m_type;
		return *this;
	}

	const FileSystemTypes FileSystemTableEntry::Type() const { return p_Impl->m_type; };
	const std::uint32_t FileSystemTableEntry::Address() const { return p_Impl->m_address; };
	const std::uint32_t FileSystemTableEntry::Length() const { return p_Impl->m_length; };
	const bool FileSystemTableEntry::IsDefault() const { return (p_Impl->m_def == FileDefault::DEFAULT); };
	const std::string FileSystemTableEntry::Name() const { return p_Impl->m_name; };

	/* FILE SYSTEM TABLE */

	FileSystemTable::FileSystemTable()
	{
		m_FSTEArray = new FSTEArray();
		valid = false;
		nEntries = 0;
		version = 0;
	}

	FileSystemTable::~FileSystemTable()
	{
		delete m_FSTEArray;
	}

	FileSystemTable::FileSystemTable(const FileSystemTable& other)
	{
		this->m_FSTEArray = new FSTEArray();
		this->valid = other.valid;
		this->nEntries = other.nEntries;
		this->version = other.version;
		FSTEArray::iterator this_it = this->m_FSTEArray->begin();
		for (FSTEArray::const_iterator it = other.m_FSTEArray->cbegin(); it != other.m_FSTEArray->cend(); ++it)
		{
			*(this_it++) = (*it);
		}
	}

	FileSystemTable& FileSystemTable::operator= (const FileSystemTable& other)
	{
		if (this == &other) return *this;
		this->valid = other.valid;
		this->nEntries = other.nEntries;
		this->version = other.version;
		FSTEArray::iterator this_it = this->m_FSTEArray->begin();
		for (FSTEArray::const_iterator it = other.m_FSTEArray->cbegin(); it != other.m_FSTEArray->cend(); ++it)
		{
			*(this_it++) = (*it);
		}
		return *this;
	}

	// Initialise FST from Reader datastream (must have had magic number checked)
	bool FileSystemTable::Initialise(std::vector<std::uint8_t> FSTData)
	{
		// Bypass magic number
		std::vector<std::uint8_t>::const_iterator it = FSTData.cbegin() + 2;
		nEntries = (*it++);
		if (nEntries > MAX_FST_ENTRIES) nEntries = MAX_FST_ENTRIES;
		version = (*it++);
		if ((version != 255) && (version > FILESYSTEM_VER)) return false;
		it += 4; // Move to first entry
		FSTEArray::iterator fste_it = this->m_FSTEArray->begin();
		for (int i = 0; i < nEntries; i++)
		{
			FileSystemTypes types;
			std::uint32_t address;
			std::uint32_t length;
			FileDefault def;

			switch ((*it) & 0xF)
			{
			case 0: types = FileSystemTypes::NO_FILE; break;
			case 1: types = FileSystemTypes::COMPENSATION_TABLE; break;
			case 2: types = FileSystemTypes::TONE_BUFFER; break;
			case 3: types = FileSystemTypes::DDS_SCRIPT; break;
			case 15: types = FileSystemTypes::USER_DATA; break;
			default: types = FileSystemTypes::NO_FILE; break;
			}
			def = (((*it++) & 0x80) == 0x80) ? FileDefault::DEFAULT : FileDefault::NON_DEFAULT;
			address = static_cast<std::uint32_t>(*it++);
			address |= ((static_cast<std::uint32_t>(*it++) << 8) & 0xFF00);
			address |= ((static_cast<std::uint32_t>(*it++) << 16) & 0xFF0000);
			length = static_cast<std::uint32_t>(*it++);
			length |= ((static_cast<std::uint32_t>(*it++) << 8) & 0xFF00);
			length |= ((static_cast<std::uint32_t>(*it++) << 16) & 0xFF0000);

			unsigned char cstr[8];
			for (int j = 0; j < 8; j++)
			{
				unsigned char c = (*it);
				if (std::isprint(c)) cstr[j] = c;
				else if (c == 0) {
					while (j < 8) {
						cstr[j++] = c; it++;
					}
					continue;
				}
				else cstr[j] = ' ';
				it++;
			}
			std::string name((const char *)cstr, 8);

			FileSystemTableEntry fste(types, address, length, def, name);

			*(fste_it++) = (fste);
		}

		valid = true;
		return true;
	}

	const FileSystemTableEntry& FileSystemTable::operator[] (std::size_t idx) const
	{
		if (idx > m_FSTEArray->size()) return NullFSTE;
		return (m_FSTEArray->at(idx));
	}

	FileSystemTableEntry& FileSystemTable::operator[] (std::size_t idx)
	{
		if (idx > m_FSTEArray->size()) return NullFSTE;
		return (m_FSTEArray->at(idx));
	}

	bool FileSystemTable::CheckNewEntry(const FileSystemTableEntry& fste) const
	{
		std::uint32_t ref_first = fste.Address();
		std::uint32_t ref_last = fste.Address() + fste.Length() - 1;

		for (FileSystemIndex i = 0; i < nEntries; i++)
		{
			if (m_FSTEArray->at(i).Type() == FileSystemTypes::NO_FILE) continue;
			std::uint32_t first = m_FSTEArray->at(i).Address();
			std::uint32_t last = m_FSTEArray->at(i).Address() + m_FSTEArray->at(i).Length() - 1;
			if (last < ref_first) continue;
			if (first > ref_last) continue;
			return false;
		}
		return true;
	}

	FileSystemIndex FileSystemTable::NextFreeEntry() const
	{
		for (FileSystemIndex i = 0; i <= (int)MAX_FST_ENTRIES; i++)
		{
			if (i == (int)MAX_FST_ENTRIES) return -1;

			if (m_FSTEArray->at(i).Type() == FileSystemTypes::NO_FILE)
			{
				return i;
			}
		}
		return -1;
	}

	bool FileSystemTable::FindFreeSpace(std::uint32_t& Addr, std::uint32_t Size) const
	{
		std::uint32_t ref_first = FileSystemStartAddress;
		std::uint32_t ref_last = ref_first + Size - 1;
		
		FileSystemIndex i = 0;
		do
		{
			if (i == nEntries)
			{
				if (ref_last <= static_cast<std::uint32_t>(FileSystemEndAddress)) {
					// Found some space!
					Addr = ref_first;
					return true;
				}
				else return false;
			}
			std::uint32_t first = m_FSTEArray->at(i).Address();
			std::uint32_t last = m_FSTEArray->at(i).Address() + m_FSTEArray->at(i).Length() - 1;
			if ((last < ref_first) || (first > ref_last) || (m_FSTEArray->at(i).Type() == FileSystemTypes::NO_FILE))
			{
				i++;
			}
			else {
				// Occupied. Try again.
				i = 0;
				ref_first = last + 1;
				ref_last = ref_first + Size - 1;
			}
		} while (i <= nEntries);
		return false;
	}

	void FileSystemTable::UpdateEntryCount()
	{
		std::uint8_t Entries = 0;
		for (FileSystemIndex i = 0; i < (int)MAX_FST_ENTRIES; i++) {
			if ((*m_FSTEArray)[i].Type() != FileSystemTypes::NO_FILE) {
				Entries = i + 1;
			}
		}
		nEntries = Entries;
	}

	FileSystemIndex FileSystemTable::GetIndexFromName(const std::string& name) const
	{
		std::string name_sz = name;
		name_sz.resize(8);
		for (int i = 0; i < nEntries; i++) {
			std::string fst_name = m_FSTEArray->at(i).Name();
			fst_name.resize(name_sz.size());
			if (fst_name == name_sz)
			{
				return i;
			}
		}
		return -1;
	}




	/* FILE SYSTEM TABLE VIEWER */
	const bool FileSystemTableViewer::IsValid() const
	{
        return with_locked_value(m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        { 
		    return ims->Synth().FST().IsValid();
        }).value_or(false);
	}

	const int FileSystemTableViewer::Entries() const
	{
        return with_locked_value(m_ims, [&](std::shared_ptr<IMSSystem> ims) -> int
        { 
    		return ims->Synth().FST().Entries();
        }).value_or(false);
	}

	const FileSystemTableEntry FileSystemTableViewer::operator[](const std::size_t idx) const
	{
		return with_locked_value(m_ims, [&](std::shared_ptr<IMSSystem> ims) -> FileSystemTableEntry
        { 
            return ims->Synth().FST()[idx];
        }).value_or(FileSystemTableEntry());
	}

	std::ostream& operator <<(std::ostream& stream, const FileSystemTableViewer& fstv) {

		for (int i = 0; i < fstv.Entries(); i++) {
			stream << "FST[" << std::setfill('0') << std::setw(2) << std::dec << i << "]" <<
				std::setfill(' ') << (fstv[i].IsDefault() ? "*" : " ") <<
				" : Type " << std::setw(2) << (int)fstv[i].Type() <<
				" Addr: 0x" << std::hex << std::setw(5) << std::setfill('0') << fstv[i].Address() <<
				" Len: " << std::dec << std::setw(6) << fstv[i].Length() <<
				" Name: " << fstv[i].Name() <<
				std::endl;
		}
		return stream;
	}



	/* FILE SYSTEM TABLE READER */
	class FileSystemTableReader::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem> ims);
		~Impl();

		std::weak_ptr<IMSSystem> m_ims;
		EEPROM *eeprom;
		EEPROMSupervisor *ee;
	};

	FileSystemTableReader::Impl::Impl(std::shared_ptr<IMSSystem> ims) : m_ims(ims), eeprom(new EEPROM(ims)), ee(new EEPROMSupervisor())
	{
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_WRITE_DONE, ee);
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_READ_DONE, ee);
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_ACCESS_FAILED, ee);
	}

	FileSystemTableReader::Impl::~Impl()
	{
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_WRITE_DONE, ee);
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_READ_DONE, ee);
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_ACCESS_FAILED, ee);

		delete ee;
		delete eeprom;
	}

	FileSystemTableReader::FileSystemTableReader(std::shared_ptr<IMSSystem> ims) : p_Impl(new Impl(ims))
	{
	}

	FileSystemTableReader::~FileSystemTableReader()
	{
		delete p_Impl;
		p_Impl = nullptr;
	}

	FileSystemTable FileSystemTableReader::Readback()
	{
		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> FileSystemTable
        {         
            // Read back FileSystemTable from iMS Synthesiser
            if (!ims->Synth().IsValid()) return FileSystemTable();
            auto conn = ims->Connection();

            // Get magic number to check first entry
            HostReport *iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, 0);
            DeviceReport Resp = conn->SendMsgBlocking(*iorpt);
            delete iorpt;
            std::vector<std::uint8_t> magic;
            if (Resp.Done()) {
                magic = Resp.Payload<std::vector<std::uint8_t>>();
            }
            else return FileSystemTable();

            p_Impl->ee->Reset();
            p_Impl->eeprom->ReadEEPROM(EEPROM::TARGET::SYNTH, FileSystemTableStartAddress, FileSystemTableLength);
            while (p_Impl->ee->Busy()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (p_Impl->ee->Error()) return FileSystemTable();

            std::vector<std::uint8_t> FSTData = p_Impl->eeprom->EEPROMData<std::vector<std::uint8_t>>();
            if (FSTData.size() < 2) return FileSystemTable();
            if ((std::vector<std::uint8_t>(FSTData.cbegin(), FSTData.cbegin() + 2)) != magic) return FileSystemTable();  // check magic

            auto fst = std::make_unique<FileSystemTable>();
            if (!fst->Initialise(FSTData)) return FileSystemTable();

            // Return by value
            return (*fst);
        }).value_or(FileSystemTable());
	}




	class FileSystemTableWriter::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem> ims, const FileSystemTable& fst);
		~Impl();

		std::weak_ptr<IMSSystem> m_ims;
		const FileSystemTable m_fst;
		EEPROM *eeprom;
		EEPROMSupervisor *ee;
	};

	FileSystemTableWriter::Impl::Impl(std::shared_ptr<IMSSystem> ims, const FileSystemTable& fst) : 
		m_ims(ims), m_fst(fst), eeprom(new EEPROM(ims)), ee(new EEPROMSupervisor())
	{
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_WRITE_DONE, ee);
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_READ_DONE, ee);
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_ACCESS_FAILED, ee);
	}

	FileSystemTableWriter::Impl::~Impl()
	{
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_WRITE_DONE, ee);
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_READ_DONE, ee);
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_ACCESS_FAILED, ee);

		delete ee;
		delete eeprom;
	}

	FileSystemTableWriter::FileSystemTableWriter(std::shared_ptr<IMSSystem> ims, const FileSystemTable& fst) : p_Impl(new Impl(ims, fst))
	{
	}

	FileSystemTableWriter::~FileSystemTableWriter()
	{
		delete p_Impl;
		p_Impl = nullptr;
	}

	bool FileSystemTableWriter::Program()
	{
		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {         
            if (!ims->Synth().IsValid()) return false;
            auto conn = ims->Connection();

            // Get magic number to write first entry
            HostReport *iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, 0);
            DeviceReport Resp = conn->SendMsgBlocking(*iorpt);
            delete iorpt;
            std::vector<std::uint8_t> fst_data;
            if (Resp.Done()) {
                fst_data = Resp.Payload<std::vector<std::uint8_t>>();
            }
            else return false;

            std::uint8_t entries = static_cast<std::uint8_t>(std::min<int>(p_Impl->m_fst.Entries(), MAX_FST_ENTRIES));
            fst_data.push_back(entries);

            // Add File System version field
            fst_data.push_back(FILESYSTEM_VER);

            // 4 reserved fields
            fst_data.push_back(0xFF);
            fst_data.push_back(0xFF);
            fst_data.push_back(0xFF);
            fst_data.push_back(0xFF);

            for (FileSystemIndex i = 0; i < entries; i++) {
                const FileSystemTableEntry& fste = p_Impl->m_fst[i];
                fst_data.push_back(static_cast<std::uint8_t>(fste.Type()) | (p_Impl->m_fst[i].IsDefault() ? 0x80 : 0));
                fst_data.push_back(fste.Address() & 0xFF);
                fst_data.push_back((fste.Address() >> 8) & 0xFF);
                fst_data.push_back((fste.Address() >> 16) & 0xFF);
                fst_data.push_back(fste.Length() & 0xFF);
                fst_data.push_back((fste.Length() >> 8) & 0xFF);
                fst_data.push_back((fste.Length() >> 16) & 0xFF);

                // Add filename
                std::string name = fste.Name();
                name.resize(8);
                char * cstr = new char[name.length() + 1];
                std::strcpy(cstr, name.c_str());
                for (int i = 0; i < 8; i++)
                {
                    fst_data.push_back(cstr[i]);
                }
                delete[] cstr;
            }

            p_Impl->ee->Reset();
            p_Impl->eeprom->EEPROMData<std::vector<std::uint8_t>>(fst_data);
            p_Impl->eeprom->WriteEEPROM(EEPROM::TARGET::SYNTH, FileSystemTableStartAddress);
            while (p_Impl->ee->Busy());
            if (p_Impl->ee->Error()) return false;

            // Update local copy  
            const IMSSynthesiser& s = ims->Synth();
            ims->Synth(IMSSynthesiser(s.Model(), s.Description(), s.GetCap(), s.GetVersion(), p_Impl->m_fst, s.AddOn()));
            
            return true;
        }).value_or(false);
	}




	/* FILE SYSTEM READER */
	class FileSystemReader::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem> ims, const FileSystemIndex index);
		~Impl();

		std::weak_ptr<IMSSystem> m_ims;
		const FileSystemIndex m_index;
		EEPROM *eeprom;
		EEPROMSupervisor *ee;
	};

	FileSystemReader::Impl::Impl(std::shared_ptr<IMSSystem> ims, const FileSystemIndex index) :
		m_ims(ims), m_index(index), eeprom(new EEPROM(ims)), ee(new EEPROMSupervisor())
	{
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_WRITE_DONE, ee);
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_READ_DONE, ee);
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_ACCESS_FAILED, ee);
	}

	FileSystemReader::Impl::~Impl()
	{
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_WRITE_DONE, ee);
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_READ_DONE, ee);
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_ACCESS_FAILED, ee);

		delete ee;
		delete eeprom;
	}


	FileSystemReader::FileSystemReader(std::shared_ptr<IMSSystem> ims, const FileSystemIndex index) :
		p_Impl(new Impl(ims, index)) {};

	FileSystemReader::FileSystemReader(std::shared_ptr<IMSSystem> ims, const std::string FileName)
	{
		if (!ims->Synth().IsValid()) {
			p_Impl = new Impl(ims, 0);
		}
		else {
			int index = ims->Synth().FST().GetIndexFromName(FileName);
			p_Impl = new Impl(ims, index);
		}
	}

	FileSystemReader::~FileSystemReader()
	{
		delete p_Impl;
		p_Impl = nullptr;
	};

	bool FileSystemReader::Readback(std::vector<std::uint8_t>& data)
	{
		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {        
            // Get magic number for start of file entry
            HostReport *iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, 0);
            DeviceReport Resp = ims->Connection()->SendMsgBlocking(*iorpt);
            delete iorpt;
            std::uint16_t magic;
            if (Resp.Done()) {
                magic = Resp.Payload<std::uint16_t>();
            }
            else return false;

            const FileSystemTable& fst = ims->Synth().FST();
            std::uint32_t addr = fst[p_Impl->m_index].Address();
            std::uint32_t len = fst[p_Impl->m_index].Length();

            // Read Magic Number
            p_Impl->ee->Reset();
            p_Impl->eeprom->ReadEEPROM(EEPROM::TARGET::SYNTH, addr, 2);
            while (p_Impl->ee->Busy());
            if (p_Impl->ee->Error()) return false;
            if (magic != p_Impl->eeprom->EEPROMData<std::uint16_t>()) return false;

            // Read Data
            data.clear();
            p_Impl->ee->Reset();
            p_Impl->eeprom->ReadEEPROM(EEPROM::TARGET::SYNTH, addr + 2, len - 2);
            while (p_Impl->ee->Busy());
            if (p_Impl->ee->Error()) return false;
            data = p_Impl->eeprom->EEPROMData<std::vector<std::uint8_t>>();

            return true;
        }).value_or(false);
	}





	/* FILE SYSTEM WRITER*/
	class FileSystemWriter::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem> ims, const FileSystemTableEntry& fste, const std::vector<std::uint8_t>& file_data);
		~Impl();

		std::weak_ptr<IMSSystem> m_ims;
		const FileSystemTableEntry m_fste;
		const std::vector<std::uint8_t> m_data;
		EEPROM *eeprom;
		EEPROMSupervisor *ee;
	};

	FileSystemWriter::Impl::Impl(std::shared_ptr<IMSSystem> ims, const FileSystemTableEntry& fste, const std::vector<std::uint8_t>& file_data) :
		m_ims(ims),m_fste(fste), m_data(file_data), eeprom(new EEPROM(ims)), ee(new EEPROMSupervisor())
	{
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_WRITE_DONE, ee);
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_READ_DONE, ee);
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_ACCESS_FAILED, ee);
	}

	FileSystemWriter::Impl::~Impl()
	{
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_WRITE_DONE, ee);
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_READ_DONE, ee);
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_ACCESS_FAILED, ee);

		delete ee;
		delete eeprom;
	}

	FileSystemWriter::FileSystemWriter(std::shared_ptr<IMSSystem> ims, const FileSystemTableEntry& fste, const std::vector<std::uint8_t>& file_data) :
		p_Impl(new Impl(ims, fste, file_data)) {};

	FileSystemWriter::~FileSystemWriter()
	{
		delete p_Impl;
		p_Impl = nullptr;
	};

	FileSystemIndex FileSystemWriter::Program()
	{
		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> FileSystemIndex
        {          
            const FileSystemTable& current_fst = ims->Synth().FST();
            // Check address range requirement to ensure no address overlap
            if (!(current_fst.CheckNewEntry(p_Impl->m_fste))) return -1;

            if (!ims->Synth().IsValid()) return -1;

            auto conn = ims->Connection();

            FileSystemTable fst_new = current_fst;
            FileSystemIndex idx = fst_new.NextFreeEntry();
            if (idx == -1) return -1;  // No more free entries in FST

            // Get magic number to write first entry
            HostReport *iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, 0);
            DeviceReport Resp = conn->SendMsgBlocking(*iorpt);
            delete iorpt;
            std::uint16_t magic;
            if (Resp.Done()) {
                magic = Resp.Payload<std::uint16_t>();
            }
            else return -1;

            // Write Magic Number
            p_Impl->ee->Reset();
            p_Impl->eeprom->EEPROMData<std::uint16_t>(magic);
            p_Impl->eeprom->WriteEEPROM(EEPROM::TARGET::SYNTH, p_Impl->m_fste.Address());
            while (p_Impl->ee->Busy());
            if (p_Impl->ee->Error()) return -1;

            // Write Data
            p_Impl->ee->Reset();
            p_Impl->eeprom->EEPROMData<std::vector<std::uint8_t>>(p_Impl->m_data);
            p_Impl->eeprom->WriteEEPROM(EEPROM::TARGET::SYNTH, (p_Impl->m_fste.Address()+2));
            while (p_Impl->ee->Busy());
            if (p_Impl->ee->Error()) return -1;

            // Update File System Table
            FileSystemTableEntry fste_new (p_Impl->m_fste.Type(),
                p_Impl->m_fste.Address(), (p_Impl->m_data.size() + 2),
                (p_Impl->m_fste.IsDefault() ? FileDefault::DEFAULT : FileDefault::NON_DEFAULT),
                p_Impl->m_fste.Name());  // override length field
            fst_new[idx] = fste_new;
            fst_new.UpdateEntryCount();
            FileSystemTableWriter fstw(ims, fst_new);
            fstw.Program();

            return idx;
        }).value_or(-1);
	}




	/* FILE SYSTEM MANAGER */
	class FileSystemManager::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem>);
		~Impl();
		std::weak_ptr<IMSSystem> m_ims;
		EEPROM *eeprom;
		EEPROMSupervisor *ee;
	};

	FileSystemManager::Impl::Impl(std::shared_ptr<IMSSystem> ims) : m_ims(ims), eeprom(new EEPROM(ims)), ee(new EEPROMSupervisor())
	{
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_WRITE_DONE, ee);
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_READ_DONE, ee);
		eeprom->EEPROMEventSubscribe(EEPROMEvents::EEPROM_ACCESS_FAILED, ee);
	}

	FileSystemManager::Impl::~Impl() 
	{
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_WRITE_DONE, ee);
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_READ_DONE, ee);
		eeprom->EEPROMEventUnsubscribe(EEPROMEvents::EEPROM_ACCESS_FAILED, ee);

		delete ee;
		delete eeprom;
	}

	FileSystemManager::FileSystemManager(std::shared_ptr<IMSSystem> ims) : p_Impl(new Impl(ims)) {}

	FileSystemManager::~FileSystemManager() { delete p_Impl; p_Impl = nullptr; }

	bool FileSystemManager::Delete(FileSystemIndex index)
	{
		if (index >= (int)MAX_FST_ENTRIES) return false;

		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {  
            if (!ims->Synth().IsValid()) return false;
    		const FileSystemTable& current_fst = ims->Synth().FST();

            FileSystemTable fst_new = current_fst;

            if (fst_new[index].Type() == FileSystemTypes::NO_FILE) return false;
            FileSystemTableEntry fste_empty(FileSystemTypes::NO_FILE, 0, 0, FileDefault::NON_DEFAULT);
            fst_new[index] = fste_empty;

            fst_new.UpdateEntryCount();
            FileSystemTableWriter fstw(ims, fst_new);
            fstw.Program();

            return true;
        }).value_or(false);
	}

	bool FileSystemManager::Delete(const std::string& FileName)
	{
		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {         
            if (!ims->Synth().IsValid()) return false;
            const FileSystemTable& current_fst = ims->Synth().FST();
            int index = current_fst.GetIndexFromName(FileName);
            if (index < 0) return false;
            else return this->Delete(index);
        }).value_or(false);
	}

	bool FileSystemManager::SetDefault(FileSystemIndex index)
	{
		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {           
            if (!ims->Synth().IsValid()) return false;
            const FileSystemTable& current_fst = ims->Synth().FST();

            FileSystemTable fst_new = current_fst;

            if (fst_new[index].Type() == FileSystemTypes::NO_FILE) return false;
            const FileSystemTableEntry& fste = fst_new[index];
            FileSystemTableEntry fste_copy(fste.Type(), fste.Address(), fste.Length(), FileDefault::DEFAULT, fste.Name());
            fst_new[index] = fste_copy;

            FileSystemTableWriter fstw(ims, fst_new);
            fstw.Program();

            return true;
        }).value_or(false);
	}

	bool FileSystemManager::SetDefault(const std::string& FileName)
	{
		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {          
            if (!ims->Synth().IsValid()) return false;
            const FileSystemTable& current_fst = ims->Synth().FST();
            int index = current_fst.GetIndexFromName(FileName);
            if (index < 0) return false;
            else return this->SetDefault(index);
        }).value_or(false);
	}

	bool FileSystemManager::ClearDefault(FileSystemIndex index)
	{
		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        { 		
            if (!ims->Synth().IsValid()) return false;
            const FileSystemTable& current_fst = ims->Synth().FST();
            FileSystemTable fst_new = current_fst;

            if (fst_new[index].Type() == FileSystemTypes::NO_FILE) return false;
            const FileSystemTableEntry& fste = fst_new[index];
            FileSystemTableEntry fste_copy(fste.Type(), fste.Address(), fste.Length(), FileDefault::NON_DEFAULT, fste.Name());
            fst_new[index] = fste_copy;

            FileSystemTableWriter fstw(ims, fst_new);
            fstw.Program();

            return true;
        }).value_or(false);
	}

	bool FileSystemManager::ClearDefault(const std::string& FileName)
	{
		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        { 		
            if (!ims->Synth().IsValid()) return false;
            const FileSystemTable& current_fst = ims->Synth().FST();
            int index = current_fst.GetIndexFromName(FileName);
            if (index < 0) return false;
            else return this->ClearDefault(index);
        }).value_or(false);
	}

	bool FileSystemManager::Sanitize()
	{
		return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        { 
            if (!ims->Synth().IsValid()) return false;
            
            FileSystemTable current_fst = ims->Synth().FST();
            FileSystemTable fst_new = current_fst;

            auto conn = ims->Connection();

            // Get magic number for start of file entry
            HostReport *iorpt = new HostReport(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, 0);
            DeviceReport Resp = conn->SendMsgBlocking(*iorpt);
            delete iorpt;
            std::uint16_t magic;
            if (Resp.Done()) {
                magic = Resp.Payload<std::uint16_t>();
            }
            else return false;

            FileSystemTableEntry fste_empty(FileSystemTypes::NO_FILE, 0, 0, FileDefault::NON_DEFAULT);

            // Find the defaults and remove duplicates
            std::array<int, (int)LAST + 1> fst_defaults;
            fst_defaults.fill(-1);
            for (FileSystemIndex i = 0; i < (int)MAX_FST_ENTRIES; i++)
            {
                if (current_fst[i].Type() != FileSystemTypes::NO_FILE)
                {
                    if (current_fst[i].IsDefault()) {
                        if (fst_defaults[(int)current_fst[i].Type()] == -1) {
                            fst_defaults[(int)current_fst[i].Type()] = i;
                        }
                        else {
                            // Clear default flag
                            FileSystemTableEntry fste(current_fst[i].Type(), current_fst[i].Address(), current_fst[i].Length(), FileDefault::NON_DEFAULT, current_fst[i].Name());
                            current_fst[i] = fste;
                        }
                    }

                    // Check for overlapping files
                    FileSystemTableEntry fste = current_fst[i];
                    current_fst[i] = fste_empty;
                    if (current_fst.CheckNewEntry(fste))
                    {
                        current_fst[i] = fste;
                    }

                    // Look for Magic Number
                    p_Impl->ee->Reset();
                    p_Impl->eeprom->ReadEEPROM(EEPROM::TARGET::SYNTH, current_fst[i].Address(), 2);
                    while (p_Impl->ee->Busy());
                    if (p_Impl->ee->Error()) return false;
                    if (magic != p_Impl->eeprom->EEPROMData<std::uint16_t>())
                    {
                        current_fst[i] = fste_empty;
                    }

                }
            }

            // Reorder FST
            int index = 0;
            for (int i = 0; i < (int)MAX_FST_ENTRIES; i++)
                fst_new[i] = fste_empty;
            for (auto e : Enum<FileSystemTypes>())
            {
                if (e != FileSystemTypes::NO_FILE) {
                    // Moves default to front
                    /*if (fst_defaults[(int)e] != -1)
                    {
                        fst_new[index++] = current_fst[fst_defaults[(int)e]];
                        current_fst[fst_defaults[(int)e]] = fste_empty;
                    }*/

                    for (FileSystemIndex i = 0; i < (int)MAX_FST_ENTRIES; i++)
                    {
                        if (current_fst[i].Type() == e)
                        {
                            fst_new[index++] = current_fst[i];
                            current_fst[i] = fste_empty;
                        }
                    }
                }
            }

            fst_new.UpdateEntryCount();
            FileSystemTableWriter fstw(ims, fst_new);
            fstw.Program();

            return true;
        }).value_or(false);
	}

	bool FileSystemManager::FindSpace(std::uint32_t& addr, const std::vector<std::uint8_t>& data) const
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        { 
		    return ims->Synth().FST().FindFreeSpace(addr, (data.size() + 2));
        }).value_or(false);
	}

	bool FileSystemManager::Execute(FileSystemIndex index)
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        { 
            if (!ims->Synth().IsValid()) return false;
            if ((index < 0) || (index > ims->Synth().FST().Entries())) return false;
            if (ims->Synth().FST()[index].Type() == FileSystemTypes::NO_FILE) return false;

            auto conn = ims->Connection();

            HostReport *iorpt = new HostReport(HostReport::Actions::RUN_SCRIPT, HostReport::Dir::WRITE, static_cast<std::uint16_t>(index));
            DeviceReport Resp = conn->SendMsgBlocking(*iorpt);
            delete iorpt;
            return (Resp.Done() && !Resp.GeneralError());
        }).value_or(false);        
	}

	bool FileSystemManager::Execute(const std::string& FileName)
	{
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        { 
            if (!ims->Synth().IsValid()) return false;
            const FileSystemTable& current_fst = ims->Synth().FST();
            int index = current_fst.GetIndexFromName(FileName);
            if (index < 0) return false;
            else return this->Execute(index);
        }).value_or(false);   
    }



	/* USER FILE READER */
	class UserFileReader::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem> ims, const FileSystemIndex index);
		Impl(std::shared_ptr<IMSSystem> ims, const std::string& FileName);
		~Impl();
		FileSystemReader* fsr;
	};

	UserFileReader::Impl::Impl(std::shared_ptr<IMSSystem> ims, const FileSystemIndex index) : fsr(new FileSystemReader(ims, index)) {}

	UserFileReader::Impl::Impl(std::shared_ptr<IMSSystem> ims, const std::string& FileName) : fsr(new FileSystemReader(ims, FileName)) {}

	UserFileReader::Impl::~Impl() { delete fsr; fsr = nullptr; }

	UserFileReader::UserFileReader(std::shared_ptr<IMSSystem> ims, const FileSystemIndex index) :
		p_Impl(new Impl(ims, index)) {};

	UserFileReader::UserFileReader(std::shared_ptr<IMSSystem> ims, const std::string& FileName) :
		p_Impl(new Impl(ims, FileName)) {};

	UserFileReader::~UserFileReader() {	delete p_Impl; p_Impl = nullptr; };

	bool UserFileReader::Readback(std::vector<std::uint8_t>& data)
	{
		return p_Impl->fsr->Readback(data);
	}





	/* USER FILE WRITER*/
	class UserFileWriter::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem> ims, const std::vector<std::uint8_t>& file_data, const std::string file_name);
		~Impl();

		const std::vector<std::uint8_t> m_data;
		const std::string m_name;
		FileSystemWriter* fsw;
	};

	UserFileWriter::Impl::Impl(std::shared_ptr<IMSSystem> ims, const std::vector<std::uint8_t>& file_data, const std::string file_name) :
		m_data(file_data), m_name(file_name)
	{
		FileSystemManager fsm(ims);
		std::uint32_t addr;
		fsm.FindSpace(addr, m_data);
		FileSystemTableEntry fste(FileSystemTypes::USER_DATA, addr, m_data.size(), FileDefault::NON_DEFAULT, m_name);
		fsw = new FileSystemWriter(ims, fste, m_data);
	}

	UserFileWriter::Impl::~Impl()
	{
		delete fsw;
	}

	UserFileWriter::UserFileWriter(std::shared_ptr<IMSSystem> ims, const std::vector<std::uint8_t>& file_data, const std::string file_name) :
		p_Impl(new Impl(ims, file_data, file_name)) {};

	UserFileWriter::~UserFileWriter() {	delete p_Impl; p_Impl = nullptr; };

	FileSystemIndex UserFileWriter::Program()
	{
		return p_Impl->fsw->Program();
	}


}
