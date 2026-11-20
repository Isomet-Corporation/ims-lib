/*-----------------------------------------------------------------------------
/ Title      : Host Protocol v2 codec
/ Project    : Isomet Modular Synthesiser System
/------------------------------------------------------------------------------
/ Description: See ProtocolV2.h and 09-26-015-SPEC-001.
/------------------------------------------------------------------------------
/ Copyright (c) 2026 Isomet (UK) Ltd. All Rights Reserved.
/----------------------------------------------------------------------------*/

#include "ProtocolV2.h"
#include "ReportManipulation.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <queue>
#include <sstream>

namespace iMS {
namespace ProtocolV2 {

	std::string StatusName(std::uint8_t status)
	{
		switch (static_cast<Status>(status)) {
		case Status::OK: return "OK";
		case Status::BAD_VERSION: return "BAD_VERSION";
		case Status::BAD_TYPE: return "BAD_TYPE";
		case Status::NO_TARGET: return "NO_TARGET";
		case Status::UNKNOWN_CMD: return "UNKNOWN_CMD";
		case Status::BAD_ADDR: return "BAD_ADDR";
		case Status::BAD_LEN: return "BAD_LEN";
		case Status::BUSY: return "BUSY";
		case Status::TARGET_TIMEOUT: return "TARGET_TIMEOUT";
		case Status::TARGET_CRC: return "TARGET_CRC";
		case Status::GENERAL_ERROR: return "GENERAL_ERROR";
		}
		std::ostringstream ss;
		ss << ((status & 0x80) ? "COMMAND_SPECIFIC_0x" : "RESERVED_0x") << std::hex << static_cast<int>(status);
		return ss.str();
	}

	// --- CRC-32C ----------------------------------------------------------------------------

	static const std::array<std::uint32_t, 256>& Crc32cTable()
	{
		static const std::array<std::uint32_t, 256> table = [] {
			std::array<std::uint32_t, 256> t{};
			for (std::uint32_t i = 0; i < 256; i++) {
				std::uint32_t c = i;
				for (int k = 0; k < 8; k++)
					c = (c & 1) ? ((c >> 1) ^ 0x82F63B78u) : (c >> 1);
				t[i] = c;
			}
			return t;
		}();
		return table;
	}

	std::uint32_t Crc32c(const std::uint8_t* data, std::size_t len)
	{
		const auto& t = Crc32cTable();
		std::uint32_t crc = 0xFFFFFFFFu;
		for (std::size_t i = 0; i < len; i++)
			crc = t[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
		return crc ^ 0xFFFFFFFFu;
	}

	// --- COBS -------------------------------------------------------------------------------

	std::vector<std::uint8_t> CobsEncode(const std::vector<std::uint8_t>& raw)
	{
		std::vector<std::uint8_t> out;
		out.reserve(raw.size() + raw.size() / 254 + 2);

		std::size_t codeIdx = 0;
		out.push_back(0);   // placeholder for the first code byte
		std::uint8_t code = 1;
		for (std::size_t i = 0; i < raw.size(); i++) {
			if (raw[i] == 0) {
				out[codeIdx] = code;
				codeIdx = out.size();
				out.push_back(0);
				code = 1;
			}
			else {
				out.push_back(raw[i]);
				if (++code == 0xFF) {
					// Full block of 254 data bytes; start another only if data remains
					out[codeIdx] = code;
					if (i + 1 < raw.size()) {
						codeIdx = out.size();
						out.push_back(0);
						code = 1;
					}
					else {
						return out;
					}
				}
			}
		}
		out[codeIdx] = code;
		return out;
	}

	bool CobsDecode(const std::uint8_t* enc, std::size_t len, std::vector<std::uint8_t>& raw)
	{
		raw.clear();
		raw.reserve(len);
		std::size_t i = 0;
		while (i < len) {
			const std::uint8_t code = enc[i++];
			if (code == 0) return false;
			const std::size_t n = static_cast<std::size_t>(code) - 1;
			if (i + n > len) return false;
			for (std::size_t k = 0; k < n; k++) {
				if (enc[i + k] == 0) return false;
				raw.push_back(enc[i + k]);
			}
			i += n;
			if ((code != 0xFF) && (i < len)) raw.push_back(0);
		}
		return true;
	}

	std::size_t MaxEncodedLen(std::size_t maxPayload)
	{
		const std::size_t raw = HeaderLen + maxPayload + CrcLen;
		return raw + raw / 254 + 1;
	}

	// --- Frames -----------------------------------------------------------------------------

	static void Put16(std::vector<std::uint8_t>& v, std::uint16_t x)
	{
		v.push_back(static_cast<std::uint8_t>(x & 0xFF));
		v.push_back(static_cast<std::uint8_t>(x >> 8));
	}

	static void Put32(std::vector<std::uint8_t>& v, std::uint32_t x)
	{
		for (int i = 0; i < 4; i++) v.push_back(static_cast<std::uint8_t>((x >> (8 * i)) & 0xFF));
	}

	std::vector<std::uint8_t> BuildFrame(Header hdr, const std::vector<std::uint8_t>& payload)
	{
		hdr.len = static_cast<std::uint16_t>(payload.size());

		std::vector<std::uint8_t> raw;
		raw.reserve(HeaderLen + payload.size() + CrcLen);
		raw.push_back(hdr.pver);
		raw.push_back(hdr.type);
		Put16(raw, hdr.seq);
		raw.push_back(hdr.cmd);
		raw.push_back(hdr.subcmd);
		raw.push_back(hdr.flags);
		raw.push_back(hdr.status);
		Put32(raw, hdr.addr);
		Put16(raw, hdr.len);
		Put16(raw, hdr.rdlen);
		raw.insert(raw.end(), payload.begin(), payload.end());
		Put32(raw, Crc32c(raw.data(), raw.size()));

		std::vector<std::uint8_t> enc = CobsEncode(raw);
		std::vector<std::uint8_t> wire;
		wire.reserve(enc.size() + 2);
		wire.push_back(Delimiter);
		wire.insert(wire.end(), enc.begin(), enc.end());
		wire.push_back(Delimiter);
		return wire;
	}

	const char* DecodeResultName(DecodeResult r)
	{
		switch (r) {
		case DecodeResult::OK: return "OK";
		case DecodeResult::BAD_COBS: return "invalid COBS encoding";
		case DecodeResult::TOO_SHORT: return "too short";
		case DecodeResult::BAD_CRC: return "CRC mismatch";
		case DecodeResult::BAD_LEN: return "LEN does not match payload";
		}
		return "?";
	}

	DecodeResult DecodeFrame(const std::uint8_t* enc, std::size_t len, Header& hdr, std::vector<std::uint8_t>& payload)
	{
		std::vector<std::uint8_t> raw;
		if (!CobsDecode(enc, len, raw)) return DecodeResult::BAD_COBS;
		if (raw.size() < HeaderLen + CrcLen) return DecodeResult::TOO_SHORT;

		const std::size_t body = raw.size() - CrcLen;
		const std::uint32_t rxCrc = static_cast<std::uint32_t>(raw[body]) |
			(static_cast<std::uint32_t>(raw[body + 1]) << 8) |
			(static_cast<std::uint32_t>(raw[body + 2]) << 16) |
			(static_cast<std::uint32_t>(raw[body + 3]) << 24);
		if (Crc32c(raw.data(), body) != rxCrc) return DecodeResult::BAD_CRC;

		hdr.pver = raw[0];
		hdr.type = raw[1];
		hdr.seq = static_cast<std::uint16_t>(raw[2] | (raw[3] << 8));
		hdr.cmd = raw[4];
		hdr.subcmd = raw[5];
		hdr.flags = raw[6];
		hdr.status = raw[7];
		hdr.addr = static_cast<std::uint32_t>(raw[8]) | (static_cast<std::uint32_t>(raw[9]) << 8) |
			(static_cast<std::uint32_t>(raw[10]) << 16) | (static_cast<std::uint32_t>(raw[11]) << 24);
		hdr.len = static_cast<std::uint16_t>(raw[12] | (raw[13] << 8));
		hdr.rdlen = static_cast<std::uint16_t>(raw[14] | (raw[15] << 8));
		payload.assign(raw.begin() + HeaderLen, raw.begin() + body);

		if (payload.size() != hdr.len) return DecodeResult::BAD_LEN;
		return DecodeResult::OK;
	}

	DecodeResult DecodeWireFrame(const std::vector<std::uint8_t>& wire, Header& hdr, std::vector<std::uint8_t>& payload)
	{
		std::size_t first = 0, last = wire.size();
		while ((first < last) && (wire[first] == Delimiter)) first++;
		while ((last > first) && (wire[last - 1] == Delimiter)) last--;
		return DecodeFrame(wire.data() + first, last - first, hdr, payload);
	}

	// --- Diagnostics ------------------------------------------------------------------------

	std::string CommandName(std::uint8_t cmd)
	{
		switch (cmd) {
		case 0x00: return "PLL_REF";
		case 0x01: return "RF_POWER";
		case 0x02: return "SYNTH_EEPROM";
		case 0x03: return "ASYNC_DAC";
		case 0x04: return "EXT_ADC";
		case 0x05: return "ASYNC_CONTROL";
		case 0x06: return "LUT_ENTRY";
		case 0x07: return "SYNTH_REG";
		case 0x08: return "AOD_TEMP";
		case 0x09: return "AOD_EEPROM";
		case 0x0A: return "RFA_ADC12";
		case 0x0B: return "RFA_ADC34";
		case 0x0C: return "RFA_TEMP";
		case 0x0D: return "RFA_EEPROM";
		case 0x0E: return "RUN_SCRIPT";
		case 0x0F: return "FAN_CONTROL";
		case 0x11: return "CTRLR_REG";
		case 0x12: return "CTRLR_IMAGE";
		case 0x13: return "CTRLR_SETTINGS";
		case 0x14: return "CTRLR_IMGDMA";
		case 0x15: return "CTRLR_IMGIDX";
		case 0x16: return "CTRLR_SYNDMA";
		case 0x17: return "CTRLR_SEQQUEUE";
		case 0x18: return "CTRLR_SEQPLAY";
		case 0x19: return "CTRLR_INTREN";
		case 0x20: return "FW_UPGRADE";
		case 0x21: return "WAVE_SHAPING";
		case 0x30: return "CTRLR_FW_UPGRADE";
		}
		std::ostringstream ss;
		ss << "CMD_0x" << std::hex << std::uppercase << static_cast<int>(cmd);
		return ss.str();
	}

	std::string Hex(const std::vector<std::uint8_t>& bytes)
	{
		static const char digits[] = "0123456789abcdef";
		std::string s;
		s.reserve(bytes.size() * 3);
		for (std::size_t i = 0; i < bytes.size(); i++) {
			if (i) s += ' ';
			s += digits[bytes[i] >> 4];
			s += digits[bytes[i] & 0x0F];
		}
		return s;
	}

	std::string Describe(const Header& hdr, const std::vector<std::uint8_t>& payload, std::size_t maxPayloadBytes)
	{
		std::ostringstream ss;
		if (hdr.pver != Version) ss << "PVER " << static_cast<int>(hdr.pver) << " ";
		ss << "SEQ " << hdr.seq << " ";

		switch (hdr.kind()) {
		case Kind::REQUEST: ss << "request"; break;
		case Kind::RESPONSE: ss << "response"; break;
		case Kind::INTERRUPT: ss << "interrupt"; break;
		default: ss << "kind?3"; break;
		}
		switch (static_cast<std::uint8_t>(hdr.target())) {
		case 0: ss << " ctrlr"; break;
		case 1: ss << " synth"; break;
		default: ss << " target?" << static_cast<int>(hdr.target()); break;
		}
		if (hdr.type & 0xF0) ss << " (TYPE 0x" << std::hex << static_cast<int>(hdr.type) << std::dec << ")";

		ss << " " << CommandName(hdr.cmd) << "(0x" << std::hex << std::setw(2) << std::setfill('0')
			<< static_cast<int>(hdr.cmd) << std::dec << ")";
		if (hdr.subcmd) ss << " subcmd " << static_cast<int>(hdr.subcmd);

		if (hdr.kind() != Kind::INTERRUPT) ss << ((hdr.flags & FlagRead) ? " read" : " write");
		if (hdr.kind() == Kind::RESPONSE) ss << " " << StatusName(hdr.status);
		else if (hdr.status) ss << " (STATUS 0x" << std::hex << static_cast<int>(hdr.status) << std::dec << ")";
		if (hdr.flags & FlagAlarm) ss << " ALARM";
		const std::uint8_t otherFlags = hdr.flags & static_cast<std::uint8_t>(~(FlagRead | FlagAlarm));
		if (otherFlags) ss << " (FLAGS 0x" << std::hex << static_cast<int>(hdr.flags) << std::dec << ")";

		ss << " addr 0x" << std::hex << std::setw(8) << std::setfill('0') << hdr.addr << std::dec;
		if ((hdr.kind() == Kind::REQUEST && (hdr.flags & FlagRead)) || hdr.rdlen) ss << " rdlen " << hdr.rdlen;
		ss << " len " << hdr.len;

		if (!payload.empty()) {
			ss << ": ";
			if (payload.size() > maxPayloadBytes) {
				ss << Hex(std::vector<std::uint8_t>(payload.begin(), payload.begin() + maxPayloadBytes))
					<< " ... (+" << (payload.size() - maxPayloadBytes) << " bytes)";
			}
			else {
				ss << Hex(payload);
			}
		}
		return ss.str();
	}

	// --- v1 <-> v2 mapping ------------------------------------------------------------------

	std::uint8_t CommandFromV1(const ReportFields& f)
	{
		std::uint8_t cmd = f.hdr & 0x0F;
		if (f.ID == ReportTypes::HOST_REPORT_ID_CTRLR) cmd |= 0x10;
		// Actions 0x20, 0x21 (synth) and 0x30 (controller) are sent in v1 with CONTEXT = 1.  No other
		//  action with these low bits uses CONTEXT, and 0x10 is not a controller action.
		if ((f.context == 1) && ((cmd == 0x00) || (cmd == 0x01) || (cmd == 0x10))) cmd |= 0x20;
		return cmd;
	}

	Target TargetFromV1(const ReportFields& f)
	{
		return (f.ID == ReportTypes::HOST_REPORT_ID_CTRLR) ? Target::CONTROLLER : Target::SYNTH;
	}

	bool ContextIsAddressExtension(std::uint8_t cmd)
	{
		switch (cmd) {
		case 0x02:  // SYNTH_EEPROM
		case 0x09:  // AOD_EEPROM
		case 0x0D:  // RFA_EEPROM
		case 0x12:  // CTRLR_IMAGE
			return true;
		default:
			return false;
		}
	}

	Header RequestHeaderFromV1(const ReportFields& f, std::uint16_t seq)
	{
		Header h;
		h.cmd = CommandFromV1(f);
		h.type = Header::MakeType(Kind::REQUEST, TargetFromV1(f));
		h.seq = seq;
		const bool read = (f.hdr & 0x80) != 0;
		h.flags = read ? FlagRead : 0;
		h.addr = f.addr;
		if (ContextIsAddressExtension(h.cmd))
			h.addr |= static_cast<std::uint32_t>(f.context) << 16;
		else if (!(h.cmd & 0x20))
			h.subcmd = f.context;
		h.rdlen = read ? f.len : 0;
		return h;
	}

	std::vector<std::uint8_t> EncodeRequest(const ReportFields& f, const std::vector<std::uint8_t>& payload, std::uint16_t seq)
	{
		return BuildFrame(RequestHeaderFromV1(f, seq), payload);
	}

	std::vector<std::uint8_t> ToV1DeviceFrame(const Header& hdr, const std::vector<std::uint8_t>& payload)
	{
		std::uint8_t id;
		std::uint8_t h = hdr.cmd & 0x0F;
		if (hdr.kind() == Kind::INTERRUPT) {
			id = static_cast<std::uint8_t>(ReportTypes::INTERRUPT_REPORT_ID_CTRLR);
			h |= 0x40;
		}
		else {
			id = static_cast<std::uint8_t>((hdr.target() == Target::CONTROLLER) ? ReportTypes::DEVICE_REPORT_ID_CTRLR : ReportTypes::DEVICE_REPORT_ID_SYNTH);
			if (hdr.pver != Version) h |= 0x20;   // general error
			else {
				switch (static_cast<Status>(hdr.status)) {
				case Status::OK: h |= 0x40; break;               // data OK
				case Status::TARGET_TIMEOUT: h |= 0x10; break;   // no-header timeout
				case Status::TARGET_CRC: break;                  // data error, CRC type
				default: h |= 0x20; break;                       // data error, general
				}
			}
		}
		if (hdr.flags & FlagAlarm) h |= 0x80;

		const std::uint16_t len = static_cast<std::uint16_t>(payload.size());
		std::vector<std::uint8_t> f{ id, h, hdr.subcmd,
			static_cast<std::uint8_t>(len & 0xFF), static_cast<std::uint8_t>(len >> 8),
			static_cast<std::uint8_t>(hdr.addr & 0xFF), static_cast<std::uint8_t>((hdr.addr >> 8) & 0xFF) };
		f.insert(f.end(), payload.begin(), payload.end());

		std::queue<std::uint8_t> q;
		for (auto b : f) q.push(b);
		CRCGenerator crc(q);
		f.push_back(static_cast<std::uint8_t>(crc.CRC() & 0xFF));
		f.push_back(static_cast<std::uint8_t>(crc.CRC() >> 8));
		return f;
	}

	// --- Capability discovery ---------------------------------------------------------------

	ProtocolInfo ProtocolInfo::Parse(const std::vector<std::uint16_t>& w)
	{
		ProtocolInfo info;
		if (w.size() < InfoWords || w[0] != InfoMagic) return info;
		info.valid = true;
		info.pverMax = w[1];
		info.features = static_cast<std::uint32_t>(w[2]) | (static_cast<std::uint32_t>(w[3]) << 16);
		info.maxPayload = std::max<std::uint16_t>(w[4], 64);
		info.maxInflight = std::max<std::uint16_t>(w[5], 1);
		info.replayDepth = w[6];
		info.relayMaxPayload = w[7];
		info.relayMaxInflight = w[8];
		return info;
	}

	std::string ProtocolInfo::Describe() const
	{
		if (!valid) return "v1 only";
		std::ostringstream ss;
		ss << "PVER_MAX " << pverMax << ", features 0x" << std::hex << features << std::dec
			<< ", max payload " << maxPayload << ", max in flight " << maxInflight
			<< ", replay depth " << replayDepth
			<< ", relay max payload " << relayMaxPayload << ", relay max in flight " << relayMaxInflight;
		return ss.str();
	}

} // namespace ProtocolV2
} // namespace iMS
