/*-----------------------------------------------------------------------------
/ Title      : EEPROM Operations Header
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/h/EEPROM.h $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2021-08-20 22:35:21 +0100 (Fri, 20 Aug 2021) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 489 $
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

#ifndef IMS_EEPROM_H__
#define IMS_EEPROM_H__

#include "IMSSystem.h"
#include "IEventHandler.h"

#include <cstdint>
#include <list>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>

#define LIBSPEC
#define EXPIMP_TEMPLATE

namespace iMS
{
	///
	/// \class SystemFuncEvents SystemFunc.h include\SystemFunc.h
	/// \brief All the different types of events that can be triggered by the SystemFunc class.
	///
	/// Some events contain integer parameter data which can be processed by the IEventHandler::EventAction
	/// derived method
	/// \author Dave Cowan
	/// \date 2015-11-11
	/// \since 1.0
	class EEPROMEvents
	{
	public:
		/// \enum Events List of Events raised by the Signal Path module
		enum Events {
			/// This event is raised when an EEPROM Read request has completed and the data is available for readback
			EEPROM_READ_DONE,
			/// This event is raised when an EEPROM Write request has completed
			EEPROM_WRITE_DONE,
			/// This event is raised when an EEPROM Read or Write request does not complete (due to CRC, timeout or some other error)
			EEPROM_ACCESS_FAILED,
			Count
		};
	};

	class EEPROMSupervisor : public IEventHandler
	{
	private:
		std::atomic<bool> m_ee_access_busy{ true };
		std::atomic<bool> m_error{ false };
	public:
		void EventAction(void* /*sender*/, const int message, const int /*param*/)
		{
			switch (message)
			{
			case (EEPROMEvents::EEPROM_READ_DONE) : m_ee_access_busy.store(false); break;
			case (EEPROMEvents::EEPROM_WRITE_DONE) : m_ee_access_busy.store(false); break;
			case (EEPROMEvents::EEPROM_ACCESS_FAILED) : m_error.store(true); m_ee_access_busy.store(false); break;
			}
		}
		bool Busy() const { return m_ee_access_busy.load(); };
		void Reset() { m_ee_access_busy.store(true); m_error.store(false); }
		bool Error() const { return m_error.load(); };
	};

	class EEPROM
	{
	public:
		EEPROM(const IMSSystem& ims);
		~EEPROM();
		///
		/// The iMS Synthesiser contains a 1Mbit EEPROM.  Some of it is reserved for system usage,
		/// but a significant quantity is available for general Application use.
		///
		/// If a compatible RF Amplifier and AO Device are connected, these too have 1Mbit EEPROMs and
		/// likewise, some is reserved for system use and a large part is available for general use.
		///
		/// To use the EEPROM, SystemFunc contains an internal buffer that can be freely read or written
		/// to by user code.  The amount of data that can be contained in the buffer is limited only by
		/// the size of the EEPROM and the amount of application time required to process the buffer
		/// when transferring it to/from the iMS System.
		///
		/// The buffer access methods are templated and can use any one of the following system types:
		///
		/// - \c uint8_t
		/// - \c uint16_t
		/// - \c uint32_t
		/// - \c int8_t
		/// - \c int16_t
		/// - \c int32_t
		/// - \c int
		/// - \c double
		/// - \c float
		/// - \c char
		/// - \c std::vector<...>  where ... is any of the above types
		/// - \c std::string
		///
		/// The EEPROM Buffer can be written to the hardware by specifying which EEPROM to access and to
		/// which address
		///
		/// The EEPROM Buffer can be filled with data from the hardware by specifying which EEPROM to
		/// access, from which address and how much data to read.  Any existing contents in the buffer
		/// are cleared.
		///
		/// \name User EEPROM Access
		///
		//@{
		/// \brief Defines which of the 3 1Mbit EEPROMs in the system to access
		enum class TARGET
		{
			/// Access the Synthesiser EEPROM
			SYNTH,
			/// Access the AO Device EEPROM
			AO_DEVICE,
			/// Access the RF Amplifier EEPROM
			RF_AMPLIFIER
		};
		/// \brief Write templated data to the EEPROM buffer
		///
		/// This function clears the EEPROM buffer in SystemFunc, then writes user data to it,
		/// according to the type specified by the template.
		///
		/// For example:
		///
		/// \code
		/// SystemFunc sys_func(iMS);
		///
		/// const std::string s ("A Test String!");
		/// sys_func.EEPROMData<std::string>(s);
		/// sys_func.WriteEEPROM(...);
		///
		/// const uint16_t a_number = 12345;
		/// sys_func.EEPROMData<uint16_t>(a_number);
		/// sys_func.WriteEEPROM(...);
		///
		/// std::vector<char> a_char_array;
		/// a_char_array.push_back('i');
		/// a_char_array.push_back('M');
		/// a_char_array.push_back('S');
		/// sys_func.EEPROMData<std::vector<char>>(a_char_array);
		/// sys_func.WriteEEPROM(...);
		///
		/// \endcode
		/// \param[in] data The templated data to write to the EEPROM Buffer
		/// \since 1.0
		template <typename T>
		void EEPROMData(const T& data);
		/// \brief Read templated data from the EEPROM buffer
		///
		/// This function retrieves data from the EEPROM buffer in SystemFunc that has been previously
		/// read from one of the hardware system EEPROMs.
		///
		/// The data can be read back in any of the formats that is supported.
		///
		/// For example:
		///
		/// \code
		/// SystemFunc sys_func(iMS);
		///
		/// sys_func.ReadEEPROM(...);
		/// std::string s = sys_func.EEPROMData<std::string>();
		/// std::cout << s;
		/// \endcode
		/// \return The data in the type specified by the template specification
		/// \since 1.0
		template <typename T>
		T EEPROMData() const;
		/// \brief Read EEPROM Data
		int ReadEEPROM(TARGET, unsigned int addr, std::size_t);
		/// \brief Write EEPROM Data
		bool WriteEEPROM(TARGET, unsigned int addr);
		//@}

		void EEPROMEventSubscribe(const int message, IEventHandler* handler);
		void EEPROMEventUnsubscribe(const int message, const IEventHandler* handler);
	private:
		// Makes this object non-copyable
		EEPROM(const EEPROM &);
		const EEPROM &operator =(const EEPROM &);

		class Impl;
		Impl * p_Impl;

		const IMSSystem& myiMS;
	};

}
#undef EXPIMP_TEMPLATE
#undef LIBSPEC
#endif
