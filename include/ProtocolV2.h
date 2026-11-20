/*-----------------------------------------------------------------------------
/ Title      : Host Protocol v2 codec
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ Description: Framing (COBS), CRC-32C, header encode/decode and v1 <-> v2
/              field mapping for iMS Host Protocol v2, as specified in
/              09-26-015-SPEC-001.  Free of library dependencies other than
/              the v1 report field definitions, so it can be unit tested alone.
/------------------------------------------------------------------------------
/ Copyright (c) 2026 Isomet (UK) Ltd. All Rights Reserved.
/----------------------------------------------------------------------------*/

#ifndef IMS_PROTOCOL_V2_H__
#define IMS_PROTOCOL_V2_H__

#include "IOReport.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace iMS {
namespace ProtocolV2 {

	constexpr std::uint8_t Version = 0x02;
	constexpr std::size_t HeaderLen = 16;
	constexpr std::size_t CrcLen = 4;
	constexpr std::uint8_t Delimiter = 0x00;

	// TYPE field
	enum class Kind : std::uint8_t { REQUEST = 0, RESPONSE = 1, INTERRUPT = 2, RESERVED = 3 };
	enum class Target : std::uint8_t { CONTROLLER = 0, SYNTH = 1 };

	// FLAGS field
	constexpr std::uint8_t FlagRead = 0x80;
	constexpr std::uint8_t FlagAlarm = 0x01;

	// STATUS field
	enum class Status : std::uint8_t {
		OK = 0x00,
		BAD_VERSION = 0x01,
		BAD_TYPE = 0x02,
		NO_TARGET = 0x03,
		UNKNOWN_CMD = 0x04,
		BAD_ADDR = 0x05,
		BAD_LEN = 0x06,
		BUSY = 0x07,
		TARGET_TIMEOUT = 0x08,
		TARGET_CRC = 0x09,
		GENERAL_ERROR = 0x0A
	};
	std::string StatusName(std::uint8_t status);

	// Interrupt frames carry this CMD (CTRLR_INTREN)
	constexpr std::uint8_t InterruptCmd = 0x19;

	struct Header
	{
		std::uint8_t pver{ Version };
		std::uint8_t type{ 0 };
		std::uint16_t seq{ 0 };
		std::uint8_t cmd{ 0 };
		std::uint8_t subcmd{ 0 };
		std::uint8_t flags{ 0 };
		std::uint8_t status{ 0 };
		std::uint32_t addr{ 0 };
		std::uint16_t len{ 0 };
		std::uint16_t rdlen{ 0 };

		Kind kind() const { return static_cast<Kind>(type & 0x03); }
		Target target() const { return static_cast<Target>((type >> 2) & 0x03); }
		static std::uint8_t MakeType(Kind k, Target t) {
			return static_cast<std::uint8_t>((static_cast<std::uint8_t>(k) & 0x03) | ((static_cast<std::uint8_t>(t) & 0x03) << 2));
		}
	};

	// CRC-32C (Castagnoli): reflected poly 0x82F63B78, init and final XOR 0xFFFFFFFF.
	//  Check value: Crc32c("123456789") == 0xE3069283
	std::uint32_t Crc32c(const std::uint8_t* data, std::size_t len);

	// COBS encode (no delimiters added) and decode.  Decode returns false if the encoding is invalid.
	std::vector<std::uint8_t> CobsEncode(const std::vector<std::uint8_t>& raw);
	bool CobsDecode(const std::uint8_t* enc, std::size_t len, std::vector<std::uint8_t>& raw);

	// Largest number of bytes between delimiters for a frame with up to maxPayload bytes of payload
	std::size_t MaxEncodedLen(std::size_t maxPayload);

	// Build a complete frame for the wire: 0x00 | COBS(header | payload | CRC) | 0x00.
	//  hdr.len is set from the payload.
	std::vector<std::uint8_t> BuildFrame(Header hdr, const std::vector<std::uint8_t>& payload);

	enum class DecodeResult { OK, BAD_COBS, TOO_SHORT, BAD_CRC, BAD_LEN };
	const char* DecodeResultName(DecodeResult r);
	// Decode the bytes between two delimiters.  On BAD_LEN the header is valid (CRC checked).
	DecodeResult DecodeFrame(const std::uint8_t* enc, std::size_t len, Header& hdr, std::vector<std::uint8_t>& payload);
	// Decode a complete wire frame including its delimiters (as built by BuildFrame)
	DecodeResult DecodeWireFrame(const std::vector<std::uint8_t>& wire, Header& hdr, std::vector<std::uint8_t>& payload);

	// --- Diagnostics ------------------------------------------------------------------------

	// Command name, e.g. "CTRLR_REG"; "CMD_0x3F" for unknown values
	std::string CommandName(std::uint8_t cmd);
	// One-line, human-readable description of a frame for logs, e.g.
	//  "SEQ 4660 response ctrlr CTRLR_REG(0x11) read OK addr 0x00000003 len 2: 78 56"
	//  Payloads longer than maxPayloadBytes are truncated with a count of the bytes not shown.
	std::string Describe(const Header& hdr, const std::vector<std::uint8_t>& payload, std::size_t maxPayloadBytes = 64);
	// Bytes as space-separated hex
	std::string Hex(const std::vector<std::uint8_t>& bytes);

	// --- Mapping between v1 report fields and v2 ---------------------------------------------

	// The v2 CMD for a v1 request: the HostReport::Actions value, reassembled from the report ID
	//  (bit 0x10), the HDR action nibble and, for the extended actions, CONTEXT (bit 0x20).
	std::uint8_t CommandFromV1(const ReportFields& f);
	Target TargetFromV1(const ReportFields& f);

	// Commands whose v1 CONTEXT carries address bits 23:16 (moved into ADDR in v2)
	bool ContextIsAddressExtension(std::uint8_t cmd);

	// Header for a v1 request.  READ requests put the v1 LEN (bytes requested) in RDLEN.
	Header RequestHeaderFromV1(const ReportFields& f, std::uint16_t seq);

	// Complete wire frame for a v1 request (fields + payload)
	std::vector<std::uint8_t> EncodeRequest(const ReportFields& f, const std::vector<std::uint8_t>& payload, std::uint16_t seq);

	// A v1 device frame equivalent to a v2 response or interrupt, so that the existing
	//  DeviceReport parser and callers handle it unchanged.  STATUS maps onto the v1 HDR bits:
	//  OK -> data OK; TARGET_TIMEOUT -> no-header timeout; TARGET_CRC -> CRC error; anything
	//  else -> general error.  ADDR keeps its low 16 bits.
	std::vector<std::uint8_t> ToV1DeviceFrame(const Header& hdr, const std::vector<std::uint8_t>& payload);

	// --- Capability discovery ---------------------------------------------------------------

	constexpr std::uint16_t InfoMagic = 0x50A5;
	constexpr std::size_t InfoWords = 10;
	// PROVISIONAL: register address of the protocol info block in both CTRLR_REG and SYNTH_REG
	//  spaces.  To be allocated by firmware (09-26-015-SPEC-001 open item 1); probing is off by
	//  default until it is.
	constexpr std::uint16_t InfoBaseProvisional = 0x00F0;

	enum Feature : std::uint32_t {
		FEATURE_INTR_INLINE = 1u << 0,
		FEATURE_REPLAY = 1u << 1,
		FEATURE_SYNTH_RELAY_V2 = 1u << 2,
		FEATURE_DIAG_COUNTERS = 1u << 3
	};

	struct ProtocolInfo
	{
		bool valid{ false };
		std::uint16_t pverMax{ 1 };
		std::uint32_t features{ 0 };
		std::uint16_t maxPayload{ 64 };
		std::uint16_t maxInflight{ 1 };
		std::uint16_t replayDepth{ 0 };
		std::uint16_t relayMaxPayload{ 0 };
		std::uint16_t relayMaxInflight{ 0 };

		// Parse the block read with a v1 register read (InfoWords 16-bit words).  A v1 device
		//  returns zeros, giving valid == false.
		static ProtocolInfo Parse(const std::vector<std::uint16_t>& words);
		bool SupportsV2() const { return valid && pverMax >= Version; }
		std::string Describe() const;
	};

} // namespace ProtocolV2
} // namespace iMS

#endif
