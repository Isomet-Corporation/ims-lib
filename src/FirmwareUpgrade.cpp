/*-----------------------------------------------------------------------------
/ Title      : Firmware Upgrade Functions CPP
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ File       : $URL: http://nutmeg.qytek.lan/svn/sw/trunk/09-Isomet/iMS_SDK/API/Other/src/FirmwareUpgrade.cpp $
/ Author     : $Author: dave $
/ Company    : Isomet (UK) Ltd
/ Created    : 2015-04-09
/ Last update: $Date: 2018-01-28 23:21:45 +0000 (Sun, 28 Jan 2018) $
/ Platform   :
/ Standard   : C++11
/ Revision   : $Rev: 315 $
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

#include "FirmwareUpgrade.h"
#include "IEventTrigger.h"
#include "HostReport.h"
#include "IConnectionManager.h"
#include "PrivateUtil.h"

#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

namespace iMS
{
	class FirmwareUpgradeEventTrigger :
		public IEventTrigger
	{
	public:
		FirmwareUpgradeEventTrigger() { updateCount(FirmwareUpgradeEvents::Count); }
		~FirmwareUpgradeEventTrigger() {};
	};

	FirmwareUpgradeProgress::FirmwareUpgradeProgress(std::uint8_t prog) : progress_code(prog) {}

	bool FirmwareUpgradeProgress::Started() const { return progress_code & 0x1 ? true : false; }
	bool FirmwareUpgradeProgress::InitializeOK() const { return progress_code & 0x2 ? true : false; }
	bool FirmwareUpgradeProgress::CheckIdOK() const { return progress_code & 0x4 ? true : false; }
	bool FirmwareUpgradeProgress::EnterUpgradeModeOK() const { return progress_code & 0x8 ? true : false; }
	bool FirmwareUpgradeProgress::EraseOK() const { return progress_code & 0x10 ? true : false; }
	bool FirmwareUpgradeProgress::ProgramOK() const { return progress_code & 0x20 ? true : false; }
	bool FirmwareUpgradeProgress::VerifyOK() const { return progress_code & 0x40 ? true : false; }
	bool FirmwareUpgradeProgress::LeaveUpgradeModeOK() const { return progress_code & 0x80 ? true : false; }

	FirmwareUpgradeError::FirmwareUpgradeError(std::uint8_t err) : error_code(err) {}

	bool FirmwareUpgradeError::StreamError() const { return error_code & 0x1 ? true : false; }
	bool FirmwareUpgradeError::IdCode() const { return error_code & 0x8 ? true : false; }
	bool FirmwareUpgradeError::Erase() const { return error_code & 0x10 ? true : false; }
	bool FirmwareUpgradeError::Program() const { return error_code & 0x20 ? true : false; }
	bool FirmwareUpgradeError::TimeOut() const { return error_code & 0x40 ? true : false; }
	bool FirmwareUpgradeError::Crc() const { return error_code & 0x80 ? true : false; }

	class FirmwareUpgrade::Impl
	{
	public:
		Impl(std::shared_ptr<IMSSystem>, const FirmwareUpgrade* aux, std::istream& strm);
		~Impl();

		std::weak_ptr<IMSSystem> m_ims;
		std::istream& m_strm;
		std::unique_ptr<FirmwareUpgradeEventTrigger> m_Event;
		UpgradeTarget m_target;

		bool downloaderRunning{ false };
		std::thread downloadThread;
		mutable std::mutex m_dlmutex;
		std::condition_variable m_dlcond;
		void DownloadWorker();

		void UpdateStatus();
		void UpdateFreeSpace();

		void UpgradeBegin();
		void VerifyBegin();
		void UpgradeEnd();

		void SendBuffer(std::vector<uint8_t>& buf);

		std::atomic<bool> IsDone;
		std::atomic<bool> IsError;
		std::atomic<uint8_t> ProgressCode;
		std::atomic<uint8_t> ErrorCode;
		std::atomic<uint16_t> FreeSpace;

		std::atomic<uint32_t> TransferredLength;
		uint32_t TotalLength;

		bool VerifyOnly;
	private:
		const FirmwareUpgrade * const m_parent;
	};

	FirmwareUpgrade::Impl::Impl(std::shared_ptr<IMSSystem> ims, const FirmwareUpgrade* fw, std::istream& strm) :
		m_ims(ims), m_parent(fw), m_strm(strm), m_Event(new FirmwareUpgradeEventTrigger())
	{
		IsDone.store(false);
		IsError.store(false);
		FreeSpace.store(0);
		ProgressCode.store(0);
		ErrorCode.store(0);

		downloaderRunning = true;
		downloadThread = std::thread(&FirmwareUpgrade::Impl::DownloadWorker, this);
	}

	FirmwareUpgrade::Impl::~Impl() 
	{
		// Unblock worker thread
		downloaderRunning = false;
		m_dlcond.notify_one();
		downloadThread.join();
	}

	FirmwareUpgrade::FirmwareUpgrade(std::shared_ptr<IMSSystem> ims, std::istream& strm) : p_Impl(new Impl(ims, this, strm))
	{
	}

	FirmwareUpgrade::~FirmwareUpgrade() 
	{ 
		delete p_Impl;
		p_Impl = nullptr;
	}

    bool FirmwareUpgrade::StartUpgrade(const UpgradeTarget target)
    {
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {         
            // Make sure Controller and Synthesiser are present
            if (!ims->Ctlr().IsValid()) return false;

            if (target == UpgradeTarget::SYNTHESISER) {
                if (!ims->Synth().IsValid()) return false;
                if (!ims->Synth().GetCap().RemoteUpgrade) return false;
            }
            else {
                if (!ims->Ctlr().GetCap().RemoteUpgrade) return false;

                if (ims->Ctlr().Model() == "iMSP") {
                    if (ims->Ctlr().GetVersion().revision == 98) {
                        // This is the limit for rev A Controllers
                        BOOST_LOG_SEV(lg::get(), sev::warning) << "Cannot upgrade iMSP rev A beyond 2.4.98" << std::endl;
                        return false;
                    }
                }
            }
            p_Impl->m_target = target;

            std::unique_lock<std::mutex> lck{ p_Impl->m_dlmutex, std::try_to_lock };

            if (!lck.owns_lock()) {
                // Mutex lock failed, Downloader must be busy, try again later
                return false;
            }

            auto conn = ims->Connection();
            HostReport::Actions target_action;
            if (p_Impl->m_target == UpgradeTarget::SYNTHESISER) {
                target_action = HostReport::Actions::FW_UPGRADE;
            }
            else {
                target_action = HostReport::Actions::CTRLR_FW_UPGRADE;
            }
            HostReport * iorpt = new HostReport(target_action, HostReport::Dir::READ, 2);
            DeviceReport ioresp = conn->SendMsgBlocking(*iorpt);
            delete iorpt;

            if (ioresp.Done() && !ioresp.GeneralError()) {
                p_Impl->TotalLength = ioresp.Payload<uint32_t>();
            }
            else p_Impl->TotalLength = 0;
            p_Impl->TransferredLength.store(0);

            p_Impl->IsDone.store(false);
            p_Impl->IsError.store(false);
            p_Impl->FreeSpace.store(0);
            p_Impl->ProgressCode.store(0);
            p_Impl->ErrorCode.store(0);

            p_Impl->VerifyOnly = false;

            p_Impl->m_dlcond.notify_one();
            return true;
        }).value_or(false);
	}
     
    bool FirmwareUpgrade::VerifyIntegrity(const UpgradeTarget target)
    {
        return with_locked_value(p_Impl->m_ims, [&](std::shared_ptr<IMSSystem> ims) -> bool
        {          
            // Make sure Controller and Synthesiser are present
            if (!ims->Ctlr().IsValid()) return false;

            if (target == UpgradeTarget::SYNTHESISER) {
                if (!ims->Synth().IsValid()) return false;
                if (!ims->Synth().GetCap().RemoteUpgrade) return false;
            }
            else {
                if (!ims->Ctlr().GetCap().RemoteUpgrade) return false;
            }
            p_Impl->m_target = target;

            std::unique_lock<std::mutex> lck{ p_Impl->m_dlmutex, std::try_to_lock };

            if (!lck.owns_lock()) {
                // Mutex lock failed, Downloader must be busy, try again later
                return false;
            }

            p_Impl->IsDone.store(false);
            p_Impl->IsError.store(false);
            p_Impl->FreeSpace.store(0);
            p_Impl->ProgressCode.store(0);
            p_Impl->ErrorCode.store(0);

            p_Impl->VerifyOnly = true;

            p_Impl->m_dlcond.notify_one();
            return true;
        }).value_or(false);        
	}
    
	bool FirmwareUpgrade::UpgradeDone() const { return p_Impl->IsDone.load(); }
	bool FirmwareUpgrade::UpgradeError() const { return p_Impl->IsError.load(); }

	FirmwareUpgradeProgress FirmwareUpgrade::GetUpgradeProgress() const {
		return FirmwareUpgradeProgress(p_Impl->ProgressCode.load());
	}

	FirmwareUpgradeError FirmwareUpgrade::GetUpgradeError() const {
		return FirmwareUpgradeError(p_Impl->ErrorCode.load());
	}

	uint32_t FirmwareUpgrade::GetTransferLength() const {
		return p_Impl->TransferredLength.load();
	}

	uint32_t FirmwareUpgrade::GetTotalTransferLength() const {
		// Send total or default if 0
		return (p_Impl->TotalLength) ? p_Impl->TotalLength : 0x220000;
	}

	void FirmwareUpgrade::FirmwareUpgradeEventSubscribe(const int message, IEventHandler* handler)
	{
		p_Impl->m_Event->Subscribe(message, handler);
	}

	void FirmwareUpgrade::FirmwareUpgradeEventUnsubscribe(const int message, const IEventHandler* handler)
	{
		p_Impl->m_Event->Unsubscribe(message, handler);
	}

	void FirmwareUpgrade::Impl::UpdateStatus()
	{
        auto ims = m_ims.lock();
        if (!ims) return;
        auto conn = ims->Connection();
                
		HostReport::Actions target_action;
		if (m_target == UpgradeTarget::SYNTHESISER) {
			target_action = HostReport::Actions::FW_UPGRADE;
		}
		else {
			target_action = HostReport::Actions::CTRLR_FW_UPGRADE;
		}
		HostReport * iorpt = new HostReport(target_action, HostReport::Dir::READ, 0);
		DeviceReport ioresp = conn->SendMsgBlocking(*iorpt);
		delete iorpt;

		if (ioresp.Done() && !ioresp.GeneralError() && (ioresp.Payload<std::vector<std::uint8_t>>().size() >= 4)) {
			bool WasDone = IsDone.load();
			if (ioresp.Payload<std::vector<std::uint8_t>>()[0] == 0) {
				IsDone.store(false);
			}
			else {
				IsDone.store(true);
				if (!WasDone) m_Event->Trigger<int>((void *)m_parent, FirmwareUpgradeEvents::FIRMWARE_UPGRADE_DONE, 0);
			}
			bool WasError = IsError.load();
			if (ioresp.Payload<std::vector<std::uint8_t>>()[1] == 0) {
				IsError.store(false);
			}
			else {
				IsError.store(true);
				if (!WasError) m_Event->Trigger<int>((void *)m_parent, FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ERROR, 0);
			}

			auto pc = ProgressCode.load();
			ProgressCode.store(ioresp.Payload<std::vector<std::uint8_t>>()[2]);
			pc ^= ProgressCode.load();
			if (pc) {
				if (pc & 0x1) m_Event->Trigger<int>((void *)m_parent, FirmwareUpgradeEvents::FIRMWARE_UPGRADE_STARTED, 0);
				if (pc & 0x2) m_Event->Trigger<int>((void *)m_parent, FirmwareUpgradeEvents::FIRMWARE_UPGRADE_INITIALIZE_OK, 0);
				if (pc & 0x4) m_Event->Trigger<int>((void *)m_parent, FirmwareUpgradeEvents::FIRMWARE_UPGRADE_CHECKID_OK, 0);
				if (pc & 0x8) m_Event->Trigger<int>((void *)m_parent, FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ENTER_UG_MODE, 0);
				if (pc & 0x10) m_Event->Trigger<int>((void *)m_parent, FirmwareUpgradeEvents::FIRMWARE_UPGRADE_ERASE_OK, 0);
				if (pc & 0x20) m_Event->Trigger<int>((void *)m_parent, FirmwareUpgradeEvents::FIRMWARE_UPGRADE_PROGRAM_OK, 0);
				if (pc & 0x40) m_Event->Trigger<int>((void *)m_parent, FirmwareUpgradeEvents::FIRMWARE_UPGRADE_VERIFY_OK, 0);
				if (pc & 0x80) m_Event->Trigger<int>((void *)m_parent, FirmwareUpgradeEvents::FIRMWARE_UPGRADE_LEAVE_UG_MODE, 0);
			}

			ErrorCode.store(ioresp.Payload<std::vector<std::uint8_t>>()[3]);
		}
	}

	void FirmwareUpgrade::Impl::UpdateFreeSpace()
	{
        auto ims = m_ims.lock();
        if (!ims) return;
        auto conn = ims->Connection();

        HostReport::Actions target_action;
		if (m_target == UpgradeTarget::SYNTHESISER) {
			target_action = HostReport::Actions::FW_UPGRADE;
		}
		else {
			target_action = HostReport::Actions::CTRLR_FW_UPGRADE;
		}
		HostReport * iorpt = new HostReport(target_action, HostReport::Dir::READ, 1);
		DeviceReport ioresp = conn->SendMsgBlocking(*iorpt);
		delete iorpt;

		if (ioresp.Done() && !ioresp.GeneralError()) {
			FreeSpace.store(ioresp.Payload<std::uint16_t>());
		}
	}

	void FirmwareUpgrade::Impl::UpgradeBegin()
	{
        auto ims = m_ims.lock();
        if (!ims) return;
        auto conn = ims->Connection();

        HostReport::Actions target_action;
		if (m_target == UpgradeTarget::SYNTHESISER) {
			target_action = HostReport::Actions::FW_UPGRADE;
		}
		else {
			target_action = HostReport::Actions::CTRLR_FW_UPGRADE;
		}
		HostReport * iorpt = new HostReport(target_action, HostReport::Dir::WRITE, 0);
		iorpt->Payload<uint8_t>(0);
		conn->SendMsg(*iorpt);
		delete iorpt;
	}

	void FirmwareUpgrade::Impl::VerifyBegin() 
	{
        auto ims = m_ims.lock();
        if (!ims) return;
        auto conn = ims->Connection();

        HostReport::Actions target_action;
		if (m_target == UpgradeTarget::SYNTHESISER) {
			target_action = HostReport::Actions::FW_UPGRADE;
		}
		else {
			target_action = HostReport::Actions::CTRLR_FW_UPGRADE;
		}
		HostReport * iorpt = new HostReport(target_action, HostReport::Dir::WRITE, 0);
		iorpt->Payload<uint8_t>(2);
		conn->SendMsg(*iorpt);
		delete iorpt;
	}

	void FirmwareUpgrade::Impl::UpgradeEnd()
	{
        auto ims = m_ims.lock();
        if (!ims) return;
        auto conn = ims->Connection();

        HostReport::Actions target_action;
		if (m_target == UpgradeTarget::SYNTHESISER) {
			target_action = HostReport::Actions::FW_UPGRADE;
		}
		else {
			target_action = HostReport::Actions::CTRLR_FW_UPGRADE;
		}
		HostReport * iorpt = new HostReport(target_action, HostReport::Dir::WRITE, 0);
		iorpt->Payload<uint8_t>(3);
		conn->SendMsg(*iorpt);
		delete iorpt;
	}

	void FirmwareUpgrade::Impl::SendBuffer(std::vector<uint8_t>& buf)
	{
        auto ims = m_ims.lock();
        if (!ims) return;
        auto conn = ims->Connection();

        HostReport::Actions target_action;
		if (m_target == UpgradeTarget::SYNTHESISER) {
			target_action = HostReport::Actions::FW_UPGRADE;
		}
		else {
			target_action = HostReport::Actions::CTRLR_FW_UPGRADE;
		}
		HostReport * iorpt = new HostReport(target_action, HostReport::Dir::WRITE, 1);
		iorpt->Payload<std::vector<uint8_t>>(buf);
		ReportFields f = iorpt->Fields();
		f.len = static_cast<std::uint16_t>(buf.size());
		iorpt->Fields(f);
		/*MessageHandle h =*/ conn->SendMsgBlocking(*iorpt);  // Use Blocking function to ensure monotonic transfer

		if (!m_strm.eof()) {
			auto tfr = TransferredLength.load();
			tfr += buf.size();
			TransferredLength.store(tfr);
		}

		delete iorpt;
	}

	void FirmwareUpgrade::Impl::DownloadWorker()
	{
		std::vector<uint8_t> buffer(IOReport::PAYLOAD_MAX_LENGTH);
		
		while (downloaderRunning) {
			std::unique_lock<std::mutex> lck{ m_dlmutex };
			m_dlcond.wait(lck);

			// Allow thread to terminate 
			if (!downloaderRunning) break;

			// Send an End Command to reset upgrade process
			UpgradeEnd();

			// Then start again
			if (VerifyOnly) {
				VerifyBegin();
			}
			else {
				UpgradeBegin();
			}

			while (!m_parent->UpgradeDone() && !m_parent->UpgradeError())
			{
				if (m_parent->GetUpgradeProgress().EraseOK() && !m_parent->GetUpgradeProgress().ProgramOK()) {
					if (FreeSpace.load() < IOReport::PAYLOAD_MAX_LENGTH) {
						UpdateStatus();
						UpdateFreeSpace();
					}

					// After Erase has completed, start transferring data
					uint16_t tfr_len = FreeSpace.load();

					buffer.clear();
					if (tfr_len > 0) {
						if (!m_strm.good() && !m_strm.eof()) {
							// Some unspecified error in the stream, abort
							IsDone.store(true);
							IsError.store(true);
							ErrorCode.store(1);
							break;
						}

						if (tfr_len > IOReport::PAYLOAD_MAX_LENGTH)
							tfr_len = IOReport::PAYLOAD_MAX_LENGTH;
						tfr_len -= (tfr_len % 16); // Maximum MCS record size is 16 bytes

						if (m_strm.eof()) {
							// Reached end of file but not finished programming yet, so zero pad
							std::fill(buffer.begin(), buffer.end(), '\0');
						}
						else {
							/*if (!m_strm.read((char *)&buffer[0], tfr_len)) {
								int tfr = m_strm.gcount();
								if (tfr) {
									tfr_len = (tfr + 3) % 4; // round up
								}
								else {
									// stream in an error state
									continue;
								}
							}*/ // This only works for binary input
							int buf_size = tfr_len;
							do {
								std::string linebuf;
								uint32_t line_size, addr, type, data;
								std::getline(m_strm, linebuf);

								if (linebuf[0] == ':') {
									errno = 0;
									line_size = strtoul(linebuf.substr(1, 2).c_str(), NULL, 16);
									addr = strtoul(linebuf.substr(3, 4).c_str(), NULL, 16);
									type = strtoul(linebuf.substr(7, 2).c_str(), NULL, 16);
									if (errno) {
										// Some error in decoding data from the stream, abort
										IsDone.store(true);
										IsError.store(true);
										ErrorCode.store(1);
										break;
									}
									else if ((type == 0) && !(line_size % 4)) {
										// Data Record
										for (unsigned int i = 0; i < (line_size / 4); i++) {
											data = strtoul(linebuf.substr(9 + i * 8, 8).c_str(), NULL, 16);
											unsigned char arr[5];
											UIntToPChar<uint32_t>((char *)arr, data);
											buffer.insert(buffer.end(), arr, &arr[4]);
											buf_size -= sizeof(data);
										}
									}
								}
							} while ((buf_size > 15) && (m_strm.good()));
						}

						// Set if data decode failed
						if (IsDone.load()) break;

						buffer.resize(tfr_len);
						SendBuffer(buffer);
						auto fs = FreeSpace.load();
						fs -= tfr_len;
						FreeSpace.store(fs);
					}
				}
				else {
					UpdateStatus();
					std::this_thread::sleep_for(std::chrono::milliseconds(25));
				}
			}
			UpgradeEnd();

			// Release lock, wait for next download trigger
			lck.unlock();
		}
	}

}
