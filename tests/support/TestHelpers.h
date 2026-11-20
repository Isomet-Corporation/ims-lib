// Helpers shared by the connection tests: an event counter, request builders and a fixture
//	that provides a connected CM_TEST.
#ifndef IMS_TEST_HELPERS_H__
#define IMS_TEST_HELPERS_H__

#include <gtest/gtest.h>

#include "CM_TEST.h"
#include "HostReport.h"
#include "IEventHandler.h"
#include "MessageEvent.h"

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <ostream>

namespace iMS::test {

	class EventCounter : public IEventHandler
	{
	public:
		// Events come with one int, two ints, or an int and data; count them all
		void EventAction(void*, const int message, const int) override { Record(message); }
		void EventAction(void*, const int message, const int, const int) override { Record(message); }
		void EventAction(void*, const int message, const int, const std::vector<std::uint8_t>) override { Record(message); }
		using IEventHandler::EventAction;
		int Count(int message)
		{
			std::scoped_lock l(m_mtx);
			return m_counts[message];
		}
		// CM_Common sets a message's status first and fires its events afterwards, at the end of
		//	its processing loop, so a test must wait for the event rather than check at once
		bool WaitFor(int message, int count, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
		{
			std::unique_lock l(m_mtx);
			return m_cv.wait_for(l, timeout, [&] { return m_counts[message] >= count; });
		}
		void Reset()
		{
			std::scoped_lock l(m_mtx);
			m_counts.clear();
		}
	private:
		void Record(int message)
		{
			{
				std::scoped_lock l(m_mtx);
				m_counts[message]++;
			}
			m_cv.notify_all();
		}

		std::mutex m_mtx;
		std::condition_variable m_cv;
		std::map<int, int> m_counts;
	};

	// The message protocol a test runs under
	enum class Protocol { V1, V2 };
	inline const char* Name(Protocol p) { return p == Protocol::V1 ? "V1" : "V2"; }
	[[maybe_unused]] inline std::ostream& operator<<(std::ostream& os, Protocol p) { return os << Name(p); }

	inline HostReport LutWrite(std::uint16_t addr, const Bytes& data = Bytes(8, 0x11))
	{
		HostReport r(HostReport::Actions::LUT_ENTRY, HostReport::Dir::WRITE, addr);
		r.Payload<Bytes>(data);
		return r;
	}

	inline HostReport SynthRegWrite(std::uint16_t addr, std::uint16_t value)
	{
		HostReport r(HostReport::Actions::SYNTH_REG, HostReport::Dir::WRITE, addr);
		r.Payload<Bytes>(Bytes{ static_cast<std::uint8_t>(value & 0xFF), static_cast<std::uint8_t>(value >> 8) });
		return r;
	}

	inline HostReport SynthRegRead(std::uint16_t addr)
	{
		HostReport r(HostReport::Actions::SYNTH_REG, HostReport::Dir::READ, addr);
		ReportFields f = r.Fields();
		f.len = 2;
		r.Fields(f);
		return r;
	}

	inline HostReport CtrlrRegRead(std::uint16_t addr)
	{
		HostReport r(HostReport::Actions::CTRLR_REG, HostReport::Dir::READ, addr);
		ReportFields f = r.Fields();
		f.len = 2;
		r.Fields(f);
		return r;
	}

	inline HostReport CtrlrWrite(HostReport::Actions action, std::uint16_t addr)
	{
		HostReport r(action, HostReport::Dir::WRITE, addr);
		r.Payload<Bytes>(Bytes(2, 0));
		return r;
	}

	// The 16-bit value carried by a device reply
	inline Bytes Value16(std::uint16_t v)
	{
		return Bytes{ static_cast<std::uint8_t>(v & 0xFF), static_cast<std::uint8_t>(v >> 8) };
	}

	inline Bytes operator+(Bytes a, const Bytes& b)
	{
		a.insert(a.end(), b.begin(), b.end());
		return a;
	}

	// A connected CM_TEST with short timeouts and every message event counted
	class CmTest : public ::testing::Test
	{
	protected:
		static constexpr int RxTimeoutMs = 200;

		CM_TEST conn;
		EventCounter events;

		void SetUp() override
		{
			conn.SetTimeouts(500, RxTimeoutMs, 10000, 0);
			for (int e = 0; e < MessageEvents::Count; e++) conn.MessageEventSubscribe(e, &events);
			conn.Connect("");
			ASSERT_TRUE(conn.Open());
		}

		void TearDown() override
		{
			conn.Disconnect();
			for (int e = 0; e < MessageEvents::Count; e++) conn.MessageEventUnsubscribe(e, &events);
		}

		// Run the rest of the test under the given protocol (v2 without the probe)
		void UseProtocol(Protocol p)
		{
			if (p == Protocol::V2) conn.UseProtocolV2();
		}

		// The value in a completed message's reply
		std::uint16_t ValueOf(MessageHandle h)
		{
			return conn.Response(h).Payload<std::vector<std::uint16_t>>().at(0);
		}
	};

}

#endif
