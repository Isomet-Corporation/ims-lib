# Request/response matching: test report

**Library:** iMS Control Library (ims-lib), protocol v1, matching code as of v2.0.10
**Test suite:** `tests/test_rx_matching.cpp`, run by CTest and by CI on Windows and Linux
**Coverage figures:** CI run 44768131859, 20 November 2026 (Linux, gcc, gcovr)

## What was tested

The host sends requests to an iMS device and has to pair each reply with the request it answers. Protocol v1 replies carry no sequence number, so the library matches each reply to the oldest outstanding request with the same report type, action and (where the device echoes it) address. Version 2.0.10 changed this matching so that it survives communication errors.

These tests show what happens to that matching **before, during and after communication errors**. Each test passes only if both of these hold:

1. **No reply is given to the wrong request.** Every read in the tests goes to its own address, and the simulated device answers with a value derived from that address. A reply given to the wrong request would show up as wrong data and fail the test.
2. **Communication recovers.** Requests made after the error complete normally, with their own data.

The tests run the library's real matching code. Only the device is simulated: a test connection takes the place of the Ethernet or serial link, records every request sent, and lets each test decide exactly what the device sends back: normal replies, lost or corrupted replies, noise, interrupts and so on.

## Communication errors tested

Each error below is tested in two ways:
- **One at a time:** good requests, then the faulted request, then good requests. The table gives the result the faulted request must end with.
- **Pipelined:** a burst of 16 requests outstanding together with the fault in the middle, then a second, clean burst. The requests before the fault and the whole second burst must complete normally, and no reply may go to the wrong request.

| Error | What happens on the link | Faulted request's result | Result |
|---|---|---|---|
| None | Normal reply (control case) | Completed | Pass |
| Lost reply | The device's reply never arrives | Timed out | Pass |
| Late reply | The reply arrives after the timeout, just before the next reply | Timed out; the late reply is absorbed, and the next request gets its own reply | Pass |
| Corrupted reply | The reply's CRC is wrong | Reported as a corrupted reply | Pass |
| Device error | The device reports an error instead of data | Reported as a device error | Pass |
| Line noise | Junk bytes in front of the reply | Completed | Pass |
| Split reply | The reply arrives in two pieces with a gap | Completed | Pass |
| Truncated frame | Part of a reply header arrives, then nothing | Timed out; the receiver resynchronises | Pass |
| Interrupt | An unsolicited interrupt in front of the reply | Completed; the interrupt is reported | Pass |
| Stray reply | A reply to a request nobody made, in front of the real reply | Completed; the stray reply is discarded | Pass |
| Duplicate reply | The device sends its reply twice | Completed; the copy is discarded | Pass |
| Send failure | The request never leaves the host | Reported as a send error | Pass |

The device's three kinds of error reply are also tested individually: general error, device-side CRC error, and the device timing out waiting for a request. Each is reported against the right request, and the requests after it complete normally.

## Other situations tested

| Situation | Result required | Result |
|---|---|---|
| A lost reply, with another request already outstanding | The lost request is marked timed out as soon as the next reply arrives | Pass |
| Several different errors in a row | Each request ends as expected; normal traffic afterwards | Pass |
| A burst of 64 writes whose last 28 replies stall past the timeout and then arrive together with the next burst's replies (the pattern seen in Ethernet captures) | The first burst's on-time replies complete and the stalled ones time out; all 64 requests of the next burst get their own replies | Pass |
| One-request-at-a-time mode (used for older iMSP firmware) with errors | Each request ends as expected; the next request isn't held up | Pass |
| Replies of the wrong report type, wrong action or wrong address | Ignored; the request still gets its own reply | Pass |
| Controller register replies (address checked) and other controller replies (address not echoed) | Matched correctly | Pass |
| A reply carrying the interlock alarm flag | Matched to its request, with its data; the alarm is reported | Pass |
| Interrupts in the reply stream, and on the separate interrupt channel, including a corrupted one | Replies around them are matched correctly | Pass |
| A reply when no request is outstanding | Discarded; the next request gets its own reply | Pass |
| Completed requests being freed from memory | Matching carries on normally | Pass |
| 150 requests, one at a time, with random errors (about 1 in 3) | Every request ends as expected; no reply given to the wrong request | Pass |
| 6 bursts of 24 requests with random errors | No reply given to the wrong request; a clean burst afterwards completes normally | Pass |
| Two identical requests, where the first's reply arrives late, just before the second's | The second request gets its own data | Pass |

The random tests use fixed seeds, so any failure can be reproduced exactly.

## Known limitations of protocol v1

The tests also show where v1 matching cannot recover straight away. **In every case below, no reply is given to the wrong request**; the effect is that requests time out for longer than they should. Each case has a test that checks today's behaviour, and a disabled test that states the behaviour wanted once it is resolved.

| Situation | Behaviour today | Recovers when |
|---|---|---|
| A loop sending the **same request repeatedly** (for example polling one register) loses one reply | Each later reply can't be told apart from a late reply to the request before it, so every request in the loop times out | A different request is sent, or the loop pauses for more than about 5 s |
| **16 or more replies lost in a row** while requests are pipelined, for example a brief link drop during a download | The host searches the 16 oldest outstanding requests for each reply. Timed-out requests keep their place for about 5 s in case their reply is late, so new replies are discarded and new requests time out too | Traffic pauses for more than about 5 s |
| A corrupted or device-error reply straight after a request that timed out | The error reply is taken by the earlier, timed-out request (a corrupted reply can't be trusted, and error replies don't echo the address), so the new request times out instead of reporting the error | The next request |

With the default Ethernet receive timeout (500 ms), requests sent one at a time don't reach the second limitation; it needs pipelined requests, or a receive timeout below about 330 ms.

Protocol v2 adds a sequence number to every request and reply, which removes the ambiguity behind these limitations.

One defect outside matching was also found: an interrupt carrying more than 4 data bytes is processed without disturbing the replies around it, but its event is not passed to the application.

## Code coverage

Measured with gcov/gcovr over the whole test suite. *Lines* is the share of executable lines that ran; *branches* is the share of decision outcomes taken.

| Code | Lines | Branches |
|---|---|---|
| **Matching code** (13 functions in `CM_Common.cpp`) | **245 / 248 (98.8%)** | **397 / 415 (95.7%)** |
| `Message.cpp` (request and reply state) | 78 / 85 (91.8%) | 42 / 48 (87.5%) |
| `ReportManipulation.cpp` (framing and CRC) | 130 / 132 (98.5%) | 70 / 76 (92.1%) |
| `DeviceReport.cpp` (reply fields and flags) | 40 / 50 (80.0%) | 20 / 35 (57.1%) |

The matching code is the set of functions in `CM_Common.cpp` that frame incoming bytes into replies, match replies to requests, and handle timeouts:

| Function | Lines | Branches |
|---|---|---|
| `SendMsg` | 11 / 11 (100%) | 7 / 7 (100%) |
| `AwaitCompletionIfSerialised` | 3 / 3 (100%) | 8 / 9 (88.9%) |
| `ExtractFrame` | 33 / 33 (100%) | 42 / 42 (100%) |
| `ResponseMatches` | 17 / 18 (94.4%) | 19 / 20 (95.0%) |
| `OutstandingMessages` | 25 / 25 (100%) | 18 / 19 (94.7%) |
| `MarkResponseLost` | 10 / 10 (100%) | 23 / 24 (95.8%) |
| `RouteFrame` | 35 / 35 (100%) | 80 / 80 (100%) |
| `ProcessRxStream` | 14 / 14 (100%) | 25 / 25 (100%) |
| `HandleMessage` | 9 / 11 (81.8%) | 17 / 22 (77.3%) |
| `HandleResponseDone` | 31 / 31 (100%) | 76 / 83 (91.6%) |
| `HandleUnexpectedChar` | 9 / 9 (100%) | 9 / 9 (100%) |
| `HandleTimeoutsAndCleanup` | 22 / 22 (100%) | 51 / 53 (96.2%) |
| `MessageListManager` | 26 / 26 (100%) | 22 / 22 (100%) |

The three lines not covered can't be reached by Ethernet or serial traffic:
- One line in `ResponseMatches` rejects a request that is neither a synthesiser nor a controller request. The library never creates such a request.
- Two lines in `HandleMessage` handle a partial reply delivered directly to a request. Only the USB connection delivers replies that way. Ethernet and serial replies arrive in one shared stream and are only handed on as complete frames.

The 18 branch outcomes not taken fall into four groups:
- **7: combinations of error flags.** These are in the test that decides whether a reply reports an error. Each kind of error reply is tested, but not every combination of the three error flags in one reply.
- **7: partial replies delivered directly to a request.** As above, only the USB connection does this.
- **1: the defensive request-type check** above.
- **3: timing edge cases.** For example, the connection closing while a request is waiting, or a timed-out request becoming complete in the same instant it would be marked lost.

Code coverage shows that this code was run by the tests; the checks inside each test show that it behaved correctly.

## Reproducing the results

```
cmake -S . -B build-tests -DIMSLIB_BUILD_TESTS=ON [-DIMSLIB_COVERAGE=ON] ...
cmake --build build-tests --target ims_unit_tests
ctest --test-dir build-tests --output-on-failure
```

The GitHub Actions workflow `.github/workflows/unit-tests.yml` runs the suite on Windows and Linux for every change, and a Linux coverage build publishes the full HTML coverage report with each run.
