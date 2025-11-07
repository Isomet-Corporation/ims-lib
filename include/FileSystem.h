/*-----------------------------------------------------------------------------
/ Title      : Synthesiser File System Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/FileSystem/h/FileSystem.h $
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

///
/// \file FileSystem.h
///
/// \brief Classes for reading, writing and managing the file system built into an iMS Synthesiser
///
///	The Synthesiser includes an area of non-volatile memory which is used for permanent storage of a variety
/// of different data types.
///
/// A simple filesystem structure has been defined which arranges and organises the data stored in the
/// memory, allowing the user to keep track of data files and the system to perform relevant functions on
/// the stored data, both on command by the user, and at startup through the setting of default flags.
///
/// The filesystem allows up to MAX_FST_ENTRIES different files to be stored in the data area, with each entry 
/// being one of 15 different types.  Each file can be any size up to the maximum available space in the memory.
///
/// The file types so far defined are:
/// \li COMPENSATION_TABLE: contents are used for programming the Compensation Look-Up table
/// \li TONE_BUFFER: contents are used for programming the Local Tone Buffer
/// \li DDS_SCRIPT: contents are DDSScriptRegister sequences for manual programming of the DDS
/// \li USER_DATA: has no functional use on the Synthesiser but can be used for application purposes, e.g. storing application settings, or web pages
///
/// The FileSystem has a FileSystemTable associated with it which stores the starting addresses, lengths and types
/// of each file stored in the FileSystem, along with a default flag indicating whether it should be executed at startup
/// and a short (max 8 character) filename for descriptive purposes.
///
/// One file of each type may be tagged as a Default, in which case when the Synthesiser initialises, it will attempt
/// to Execute that file.  If multiple files are tagged default, the lowest index of each type is executed and any 
/// subsequent flags cleared.
///
/// File execution as a predictable effect on each type of file, except for USER_DATA, which does nothing (can only by
/// read and written).
///
/// At present, the total size of the filesystem on all Synthesiser models is 128kB with 1kB reserved for system use.  The
/// FileSystemManager will allocate space in memory for data to be downloaded to but files
/// must always be stored contiguously therefore it is up to the user to ensure the FileSystem does not become excessively
/// fragmented.
///
/// All files stored to the FileSystem of all types are prepended with a 2-byte marker symbol which is a requirement of the
/// FileSystem protocol.
///
/// When an IMSSystem object is initialised (typically through the ConnectionList::Scan() method), the FileSystemTable is read
/// back and made available for use by classes in this file.
///
/// \author Dave Cowan
/// \date 2016-01-20
/// \since 1.1
/// \ingroup group_File
///

#ifndef IMS_FILESYSTEM_H__
#define IMS_FILESYSTEM_H__

#include "IMSSystem.h"
#include "IEventHandler.h"

#include <memory>
#include <array>

/// \cond LIB_CREATION
#if defined _WIN32 || defined __CYGWIN__
  #ifdef __GNUC__
    #define DLL_EXPORT __attribute__ ((dllexport))
    #define DLL_IMPORT __attribute__ ((dllimport))
  #else
    #define DLL_EXPORT __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #define DLL_IMPORT __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
  #endif
  #define DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define DLL_EXPORT __attribute__ ((visibility ("default")))
    #define DLL_IMPORT __attribute__ ((visibility ("default")))
    #define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define DLL_EXPORT
    #define DLL_IMPORT
    #define DLL_LOCAL
  #endif
#endif

#if defined(_EXPORTING_IMS)
  #define LIBSPEC DLL_EXPORT
  #define LIBLOCAL DLL_LOCAL
  #define EXPIMP_TEMPLATE
#elif defined(_STATIC_IMS)
  #define LIBSPEC
  #define LIBLOCAL
  #define EXPIMP_TEMPLATE
#else
  #define LIBSPEC DLL_IMPORT
  #define LIBLOCAL DLL_LOCAL
  #define EXPIMP_TEMPLATE extern
#endif
/// \endcond

namespace iMS
{
	/// \enum FileSystemTypes
	/// \brief All of the different (up to 15) types of file available to the filesystem
	/// \since 1.1
	enum class FileSystemTypes : std::uint8_t
	{
		/// No file stored at this FileSystemTable entry
		NO_FILE = 0,
		/// File contains Compensation table data
		COMPENSATION_TABLE = 1,
		/// File contains ToneBuffer data
		TONE_BUFFER = 2,
		/// File contains a DDS Script for manual programming of the DDS
		DDS_SCRIPT = 3,
		/// File contains user data for application use
		USER_DATA = 15
	};

	/// \enum FileDefault
	/// \brief Default flag tags a file entry for execution at startup (only one per filetype)
	/// \since 1.1
	enum class FileDefault : bool
	{
		/// Default indicates the Synthesiser should attempt to execute that file during its startup procedure
		DEFAULT = true,
		/// Non-default is the normal state for most files
		NON_DEFAULT = false
	};

	/// \brief FileSystemIndex represents the entry number for a particular file in the FileSystemTable
	using FileSystemIndex = int;

	/// \struct FileSystemTableEntry FileSystem.h include\FileSystem.h
	/// \brief Contains all the parameters that uniquely locate a File within the Synthesiser FileSystem
	///
	/// A FileSystemTableEntry object stores the length, address, file type, file name and default flag status
	/// of any file stored within the Synthesiser FileSystem.
	///
	/// It is not normally necessary for the user application to create a FileSystemTableEntry object since this will
	/// be handled by the individual File Writing method (e.g. CompensationTableDownload::Store()), by the FileSystemManager or
	/// during IMSSytem initialisation.
	/// However the struct is useful for reading parameter data about a file entry in the table using the various const methods.
	/// \author Dave Cowan
	/// \date 2016-01-20
	/// \since 1.1
	struct LIBSPEC FileSystemTableEntry
	{
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Empty Constructor for FileSystemTableEntry Object
		/// \since 1.1
		FileSystemTableEntry();
		/// \brief Constructor for FileSystemTableEntry Object with no FileName specified
		/// \param[in] type File Type of table entry
		/// \param[in] addr Address in FileSystem where table entry is stored
		/// \param[in] length number of bytes occupied by file in FileSystem
		/// \param[in] def Flag indicating whether File should be executed at startup
		/// \since 1.1
		FileSystemTableEntry(FileSystemTypes type, std::uint32_t addr, std::uint32_t length, FileDefault def);
		/// \brief Full Constructor for FileSystemTableEntry Object with FileName
		/// \param[in] type File Type of table entry
		/// \param[in] addr Address in FileSystem where table entry is stored
		/// \param[in] length number of bytes occupied by file in FileSystem
		/// \param[in] def Flag indicating whether File should be executed at startup
		/// \param[in] name 8-character string given to table entry describing the contents of the file
		/// \since 1.1
		FileSystemTableEntry(FileSystemTypes type, std::uint32_t addr, std::uint32_t length, FileDefault def, std::string name);
		/// \brief Destructor for FileSystemTableEntry
		~FileSystemTableEntry();
		/// \brief Copy Constructor
		FileSystemTableEntry(const FileSystemTableEntry &);
		/// \brief Assignment Constructor
		FileSystemTableEntry &operator =(const FileSystemTableEntry &);
		//@}

		/// \name FileSystemTable entry parameter readback
		//@{
		/// \return File Type of table entry
		const FileSystemTypes Type() const;
		/// \return Address in FileSystem memory of table entry
		const std::uint32_t Address() const;
		/// \return Length in bytes occupied in memory of table entry
		const std::uint32_t Length() const;
		/// \return true if entry is marked for execution at startup
		const bool IsDefault() const;
		/// \return string representing descriptive file name given to table entry
		const std::string Name() const;
		//@}

	private:
		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};

	/// \brief Maximum number of entries that may be stored in the FileSystem
	const unsigned int MAX_FST_ENTRIES = 33;

	///
	/// \class FileSystemTableViewer FileSystem.h include\FileSystem.h
	/// \brief Provides a mechanism for viewing the FileSystemTable associated with an iMS System
	/// \author Dave Cowan
	/// \date 2016-01-21
	/// \since 1.1
	class LIBSPEC FileSystemTableViewer
	{
	public:
		///
		/// \name Constructor
		//@{
		///
		/// \brief Constructor for FileSystemTableViewer Object
		///
		/// The FileSystemTableViewer object requires an IMSSystem object, which will have had its FileSystemTable read back
		/// during initialisation.  It must therefore exist before the
		/// FileSystemTableViewer object, and must remain valid (not destroyed) until the FileSystemTableViewer
		/// object itself is destroyed.
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// \param[in] ims A const reference to the iMS System whose FileSystemTable is to be viewed.
		/// \since 1.1
		FileSystemTableViewer(std::shared_ptr<IMSSystem> ims) : m_ims(ims) {};
		//@}

		/// \name FileSystem Table Information
		//@{
		/// \brief Indicates whether FileSystemTable object is valid
		///
		/// For a FileSystemTable stored on the Synthesiser to be considered valid, certain parameters need to be met.
		/// If the initialisation process is unable to establish validity of a FileSystemTable it will mark it as void
		/// and the user will not be able to work with it until a new FileSystem has been created and downloaded.
		///
		/// User code should therefore check that the FileSystemTable is valid before working with it.
		/// \return true if the FileSystemTable is considered valid.
		/// \since 1.1
		const bool IsValid() const;
		/// \return The current number of file entries stored in the FileSystemTable
		/// \since 1.1
		const int Entries() const;
		//@}
		/// \name Array operator for random access to FileSystemTableEntry s
		//@{
		/// \brief The FileSystemTable consists of a container of FileSysteTableEntry objects.  Each object may be
		/// accessed by calling the viewer object through an array subscript.
		///
		/// For example:
		/// \code
		/// FileSystemTableViewer fstv(m_ims);
		/// if (fstv.IsValid()) {
		///		int length = 0;
		///		for (int i=0; i<fstv.Entries(); i++) {
		///			length += fstv[i].Length();
		///		}
		///		std::cout << "Used space in filesystem: " << length << " bytes" << std::endl;
		/// }
		/// \endcode
		/// \since 1.1
		const FileSystemTableEntry operator[](const std::size_t idx) const;
		//@}

		/// \brief Stream operator overload to simplify debugging
		/// 
		/// Example usage:
		/// \code
		/// FileSystemTableViewer fstv(m_ims);
		/// if (!fstv.IsValid()) {
		///		std::cout << "No Filesystem found" << std::endl;
		///	}
		/// else {
		///		std::cout << fstv;
		/// }
		/// \endcode
		/// might produce the result:
		/// \code
		/// FST[00]* : Type  1 Addr :  8708 Len : 16386 Name : CompTbl1
		/// FST[01]  : Type  1 Addr : 38924 Len : 16386 Name : CompTbl2
		///	FST[02]* : Type  2 Addr : 1024  Len : 6146 Name : ToneUp
		///	FST[03]  : Type  2 Addr : 25094 Len : 6146 Name : ToneDown
		///	FST[04]  : Type 15 Addr : 55310 Len : 1538 Name : User5
		///	FST[05]  : Type 15 Addr : 56848 Len : 1538 Name : User5
		///	FST[06]  : Type  3 Addr : 7170  Len : 17 Name : DDS100M
		/// \endcode
		/// where The index into the FileSystemTable (FST) is given followed by an asterisk if the entry is marked as Default (Execute on startup).
		/// Then the File Type is given (refer to FileSystemTypes), followed by the starting address in memory then the number of bytes occupied
		/// and finally the allocated filename.
		friend LIBSPEC std::ostream& operator<< (std::ostream& stream, const FileSystemTableViewer&);
	private:
		// Make this object non-copyable
		FileSystemTableViewer(const FileSystemTableViewer& other);
		FileSystemTableViewer& operator= (const FileSystemTableViewer& other);

		std::weak_ptr<IMSSystem> m_ims;
	};

	///
	/// \class FileSystemManager FileSystem.h include\FileSystem.h
	/// \brief Provides user management operations for working with Synthesiser FileSystems
	/// \author Dave Cowan
	/// \date 2016-01-21
	/// \since 1.1
	class LIBSPEC FileSystemManager
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for FileSystemManager Object
		///
		/// The FileSystemManager object requires an IMSSystem object, which will have had its FileSystemTable read back
		/// during initialisation.  It must therefore exist before the
		/// FileSystemManager object, and must remain valid (not destroyed) until the FileSystemManager
		/// object itself is destroyed.
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// \param[in] ims A const reference to the iMS System whose FileSystemTable is to be operated upon.
		/// \since 1.1
		FileSystemManager(std::shared_ptr<IMSSystem> ims);
		/// \brief Destructor for FileSystemManager object
		~FileSystemManager();
		//@}
		
		/// \name File System Operations
		//@{
		/// \brief Removes the Entry indicated by the provided index from the FileSystemTable
		///
		/// The Entry is removed from the FST.  The file data itself is not overwritten but once the entry has been
		/// deleted, it is impractival to recover the FileSystem data subsequently.  The space 'freed up' by the
		/// deletion will become available for future file downloads and the release FST entry may be reused.
    ///
    /// \bug Prior to v1.2.4 it was possible to attempt to delete an entry >= MAX_FST_ENTRIES.  Doing so would have generated an exception.
    /// The condition is now checked for and the function will fail (return false) if attempted.
		/// \param[in] index the Entry in the FST to delete (from 0 to MAX_FST_ENTRIES-1).
		/// \return true if the deletion process was carried out successfully
		/// \since 1.1
		bool Delete(FileSystemIndex index);
		/// \overload
		/// Deletes a file from the FileSystemTable referencing it by its allocated filename
		/// \param[in] FileName a string representing the name of the file to delete
		/// \return true if the filename was recognised and the deletion process was carried out successfully
		/// \since 1.1
		bool Delete(const std::string& FileName);
		/// \brief Tags a File for execution at Synthesiser startup
		///
		/// A single file of each file type may be marked as being the 'default' of its type.  If tagged as such,
		/// the Synthesiser will attempt to execute the file during its initialisation process.  All file types
		/// except USER_DATA may have a default entry.
		///
		/// If multiple files are marked as default, the entry with the lowest index number will take precedence.
		/// Any subsequent files marked as default will have their flags cleared during initialisation.
		/// \param[in] index the Entry in the FST to mark as default (from 0 to MAX_FST_ENTRIES-1).
		/// \return true if the default flag was set successfully
		/// \since 1.1
		bool SetDefault(FileSystemIndex index);
		/// \overload
		/// Tags a File for execution at Synthesiser startup referencing it by its allocated filename
		/// \param[in] FileName a string representing the name of the file to mark default
		/// \return true if the filename was recognised and the default flag was set successfully
		/// \since 1.1
		bool SetDefault(const std::string& FileName);
		/// \brief Removes the Default Flag assigned to a FileSystemTableEntry
		///
		/// \param[in] index the Entry in the FST to unset as default (from 0 to MAX_FST_ENTRIES-1).
		/// \return true if the default flag was unset successfully
		/// \since 1.1
		bool ClearDefault(FileSystemIndex index);
		/// \overload
		/// Removes the tag indicating a file should be executed at startup referencing it by its allocated filename
		/// \param[in] FileName a string representing the name of the file to unset as default
		/// \return true if the filename was recognised and the default flag was unset successfully
		/// \since 1.1
		bool ClearDefault(const std::string& FileName);
		/// \brief Reorganises the FileSystemTable and ensures it contains valid contents
		///
		/// The Sanitize process will do the following:
		/// \li ensure only one default flag is set per filetype, clearing the flag set on any subsequent entries
		/// \li check that valid filesystem contents is present for each entry
		/// \li look for any filesystem contents that may overlap, removing entries that are aliased
		/// \li Reorders the FST according to FileSystemTypes with any default marked entries placed at the front
		/// \return true if the process completed successfully
		/// \since 1.1
		bool Sanitize();
		//@}

		/// \name Miscellaneous Functions
		//@{
		/// \brief Locates an area in the FileSystem memory large enough to store the provided contents
		///
		/// Given a const reference to a byte array containing the data which the caller wants to place
		/// in FileSystem memory, this function operates an algorithm that will search through the FileSystemTable
		/// iteratively searching for the lowest possible address in memory that will fit the data in a contiguous
		/// block (since the FileSystem does not support distributed storage).
		/// \param[out] addr The location in memory where the data may be safely stored
		/// \param[in] data a reference to a byte array representing the data which is to be stored
		/// \return true if the algorithm was successful, false if no space could be found
		/// \since 1.1
		bool FindSpace(std::uint32_t& addr, const std::vector<std::uint8_t>& data) const;
		
		/// \brief Causes the Synthesiser to access the FileSystem data represented by the index and execute it.
		///
		/// The execution of the FileSystem contents is defined in a FileSystemTypes specific way:
		/// \li COMPENSATION_TABLE data is loaded into the Compensation Look-Up Table
		/// \li TONE_BUFFER data is loaded into the Local Tone Buffer memory
		/// \li DDS_SCRIPT data is written register at a time to the DDS IC (the User must ensure that no Image Data is currently being played back to prevent unexpected behaviour)
		/// \li USER_DATA no action is performed
		/// \param[in] index the Entry in the FileSystemTable to operate on
		/// \return if the Execution was started successfully.
		/// \since 1.1
		bool Execute(FileSystemIndex index);
		/// \overload
		/// \param[in] FileName a string representing the name of the file to operate on
		/// \since 1.1
		bool Execute(const std::string& FileName);
		//@}
	private:
		// Make this object non-copyable
		FileSystemManager(const FileSystemManager &);
		const FileSystemManager &operator =(const FileSystemManager &);

		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class UserFileReader FileSystem.h include\FileSystem.h
	/// \brief Provides a mechanism for retrieving User File data from the Synthesiser FileSystem
	/// \author Dave Cowan
	/// \date 2016-01-21
	/// \since 1.1
	class LIBSPEC UserFileReader
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for UserFileReader Object
		///
		/// The UserFileReader object requires an IMSSystem object, which will have had its FileSystemTable read back
		/// during initialisation.  It must therefore exist before the
		/// UserFileReader object, and must remain valid (not destroyed) until the UserFileReader
		/// object itself is destroyed.  The UserFileReader object is tied to a single FileSystemTableEntry and can only
		/// be used for reading back that object.  If multiple files need to be read back, new UFRs should be created for
		/// each one.
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// \param[in] ims A const reference to the iMS System whose FileSystemTable should be used for reading back data
		/// \param[in] index the Entry in the FileSystemTable containing USER_DATA file data to readback
		/// \since 1.1
		UserFileReader(std::shared_ptr<IMSSystem> ims, const FileSystemIndex index);
		/// \brief Constructor for UserFileReader Object (referenced by File Name)
		///
		/// \param[in] ims A const reference to the iMS System whose FileSystemTable should be used for reading back data
		/// \param[in] FileName a string representing the name of the entry containing USER_DATA file data to readback
		/// \since 1.1
		UserFileReader(std::shared_ptr<IMSSystem> ims, const std::string& FileName);
		/// \brief UserFileReader destructor
		~UserFileReader();
		//@}

		/// \name Readback Core Function
		//@{
		/// \brief Retrieves User File data into a byte array
		///
		/// Call this function to initiate readback of data from the Synthesiser FileSystem into a byte array allocated
		/// by the application
		/// \param[out] data A reference to a vector to store the unformatted byte data representing the user file.  Any existing contents are overwritten.
		/// \return true if the operation was successful
		/// \since 1.1
		bool Readback(std::vector<std::uint8_t>& data);
		//@}
	private:
		// Make this object non-copyable
		UserFileReader(const UserFileReader &);
		const UserFileReader &operator =(const UserFileReader &);
		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};

	///
	/// \class UserFileWriter FileSystem.h include\FileSystem.h
	/// \brief Provides a mechanism for committing User File data to the Synthesiser FileSystem
	/// \author Dave Cowan
	/// \date 2016-01-21
	/// \since 1.1
	class LIBSPEC UserFileWriter
	{
	public:
		///
		/// \name Constructor & Destructor
		//@{
		///
		/// \brief Constructor for UserFileWriter Object
		///
		/// The UserFileWriter object requires an IMSSystem object, which will have had its FileSystemTable read back
		/// during initialisation.  It must therefore exist before the
		/// UserFileWriter object, and must remain valid (not destroyed) until the UserFileWriter
		/// object itself is destroyed. 
		///
		/// A reference to the User File data wrapped in an unformatted byte array needs to be provided, along with a
		/// string representing the file name to allocate to the file in the FileSystemTable.
		///
		/// The File Name may be any sequence of valid ASCII characters, including all special characters ($, %, /, \ etc)
		/// but not control characters.  It is limited to 8 characters and will be truncated as such.  Though not recommended,
		/// it is permissible to allocate the same filename to multiple files contained in the FileSystemTable
		///
		/// Once constructed, the object can neither be copied or assigned to another instance.
		///
		/// \param[in] ims A const reference to the iMS System whose FileSystemTable should be used for writing new data
		/// \param[in] file_data an unformatted byte array containing the User File contents to program
		/// \param[in] file_name a string representing the name of the file to be allocated in the FileSystemTable
		/// \since 1.1
		UserFileWriter(std::shared_ptr<IMSSystem> ims, const std::vector<std::uint8_t>& file_data, const std::string file_name);
		/// \brief Destructor for UserFileWriter
		~UserFileWriter();
		//@}

		/// \name File Write Core Function
		//@{
		/// \brief Stores User File data into a FileSystem and allocates a new FileSystemTableEntry
		///
		/// Call this function to initiate writing of the provided User File data into the FileSystem.
		///
		/// The function will first attempt to find sufficient free space and allocate it for the new data.  If it cannot do
		/// that, it will return an invalid FileSystemIndex (-1).  If free space was found, it will start writing the user file
		/// data starting at the address that was found by the allocation algorithm (the user application cannot predict where in the Filesystem address
		/// space the User data will be stored but this is unlikely to be a problem).  A new FileSystemTableEntry is created and
		/// added to the FileSystemTable containing the allocated address, the overall file length (including an additional 2-byte marker
		/// at the start required by the FileSystem protocol), the type as USER_DATA, a NON_DEFAULT marker, and the next available index.
		/// \return the index in the FileSystemTable that was created by the Programming process, or -1 if it failed.
		/// \since 1.1
		FileSystemIndex Program();
	private:
		// Make this object non-copyable
		UserFileWriter(const UserFileWriter &);
		const UserFileWriter &operator =(const UserFileWriter &);

		// Declare Implementation
		class Impl;
		Impl * p_Impl;
	};

}

#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
