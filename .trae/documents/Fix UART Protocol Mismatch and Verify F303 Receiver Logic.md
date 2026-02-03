I will fix the UART communication protocol mismatch between the F310 (Sender) and F303 (Receiver) boards and verify the display logic.

### 1. F310 (Sender) - Packet Protocol Update
*   **Modify `DataType.h`**: Change `SPO2_PACKET_LENGTH` from `0x06` to `0x03` (Payload Length).
    *   The current length `0x06` is incorrect because the payload (`spo2`, `heart_rate`, `pi`) is only 3 bytes. This causes the Receiver to wait for data that is never sent.
    *   Setting it to `0x03` correctly reflects the 3 data bytes.

### 2. F303 (Receiver) - Parsing Logic Fix
*   **Modify `UART2.c`**:
    *   **Length Validation**: Update `PARSE_STATE_WAIT_LENGTH` to accept a length of `0x03` (change range check from `6-16` to `3-16`).
    *   **Checksum Calculation**: Correct the checksum verification logic in `PARSE_STATE_WAIT_END`.
        *   Currently, it calculates checksum on `s_expectedLength - 2` bytes, which is incorrect.
        *   It needs to calculate the checksum over `Length + Payload`.
        *   Change `CalculateChecksum` call to sum bytes starting from `s_packetBuffer[1]` (Length byte) for `s_expectedLength + 1` bytes.

### 3. Verification
*   **Code Review**: I will verify that the Sender's checksum formula (`Length + SpO2 + HR + PI`) matches the Receiver's new checksum verification logic.
*   **Functionality**: This ensures the F303 correctly receives, parses, and accepts the SPO2 data packets sent by the F310, enabling the LCD to update.
