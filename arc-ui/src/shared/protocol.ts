/**
 * Binary framing protocol for the arc-command named pipe.
 *
 * Wire format (Phase 1 — matches CommandServer.cpp):
 *   [uint32 LE: MAGIC = 0x41524345 "ARCE"]
 *   [uint32 LE: payloadLength]
 *   [uint8:     commandType]
 *   [payloadLength-1 bytes: command payload]
 *
 * Strings in payloads are null-terminated char arrays (ASCII/UTF-8).
 */

import { CommandType, type EngineCommand } from './commands'

const MAGIC = 0x41524345 // "ARCE"

/** Encode a command into a Buffer ready to write to the named pipe. */
export function encodeCommand(cmd: EngineCommand): Buffer {
  const payload = encodePayload(cmd)
  // payloadLength includes the 1-byte command type
  const payloadLength = 1 + payload.length

  const buf = Buffer.allocUnsafe(4 + 4 + 1 + payload.length)
  let offset = 0
  buf.writeUInt32LE(MAGIC, offset);         offset += 4
  buf.writeUInt32LE(payloadLength, offset); offset += 4
  buf.writeUInt8(cmd.type, offset);         offset += 1
  payload.copy(buf, offset)

  return buf
}

// ---------------------------------------------------------------------------
// Per-command payload encoders
// ---------------------------------------------------------------------------

function encodePayload(cmd: EngineCommand): Buffer {
  switch (cmd.type) {

    case CommandType.SetVolume: {
      const b = Buffer.allocUnsafe(4)
      b.writeFloatLE(cmd.gainDb, 0)
      return b
    }

    case CommandType.SetBypass: {
      return Buffer.from([cmd.bypass ? 1 : 0])
    }

    case CommandType.LoadPlugin: {
      const b = Buffer.allocUnsafe(4 + 256 + 512)
      b.fill(0)
      b.writeInt32LE(cmd.slot, 0)
      writeNullTermString(b, cmd.pluginId,   4,   256)
      writeNullTermString(b, cmd.pluginPath, 260, 512)
      return b
    }

    case CommandType.RemovePlugin: {
      const b = Buffer.allocUnsafe(4)
      b.writeInt32LE(cmd.slot, 0)
      return b
    }

    case CommandType.MovePlugin: {
      const b = Buffer.allocUnsafe(8)
      b.writeInt32LE(cmd.fromSlot, 0)
      b.writeInt32LE(cmd.toSlot, 4)
      return b
    }

    case CommandType.SetPluginBypass: {
      const b = Buffer.allocUnsafe(5)
      b.writeInt32LE(cmd.slot, 0)
      b.writeUInt8(cmd.bypass ? 1 : 0, 4)
      return b
    }

    case CommandType.SetPluginMix: {
      const b = Buffer.allocUnsafe(8)
      b.writeInt32LE(cmd.slot, 0)
      b.writeFloatLE(cmd.mix, 4)
      return b
    }

    case CommandType.SetInputGain:
    case CommandType.SetOutputGain: {
      const b = Buffer.allocUnsafe(4)
      b.writeFloatLE(cmd.gainDb, 0)
      return b
    }

    case CommandType.ScanPlugins: {
      const b = Buffer.alloc(512)
      writeNullTermString(b, cmd.path, 0, 512)
      return b
    }

    case CommandType.LoadPreset: {
      const b = Buffer.alloc(512)
      writeNullTermString(b, cmd.path, 0, 512)
      return b
    }

    case CommandType.SavePreset: {
      const b = Buffer.alloc(640)
      b.fill(0)
      writeNullTermString(b, cmd.path, 0,   512)
      writeNullTermString(b, cmd.name, 512, 128)
      return b
    }

    case CommandType.SetSampleRate: {
      const b = Buffer.allocUnsafe(8)
      b.writeDoubleBE(cmd.sampleRate, 0)  // double is 8 bytes
      return b
    }

    case CommandType.SetBufferSize: {
      const b = Buffer.allocUnsafe(4)
      b.writeInt32LE(cmd.bufferSize, 0)
      return b
    }

    case CommandType.SetOutputDevice: {
      const b = Buffer.alloc(256)
      writeNullTermString(b, cmd.deviceName, 0, 256)
      return b
    }

    case CommandType.RequestStatus:
    case CommandType.DetectVBCable:
    case CommandType.Shutdown:
      return Buffer.alloc(0)

    case CommandType.SetVBCableRouting: {
      // matches SetVBCableRoutingCmd: uint8 enable + uint8 pad[3]
      const b = Buffer.alloc(4)
      b.writeUInt8(cmd.enable ? 1 : 0, 0)
      return b
    }

    default: {
      const _exhaustive: never = cmd
      return Buffer.alloc(0)
    }
  }
}

function writeNullTermString(buf: Buffer, str: string, offset: number, maxLen: number): void {
  const encoded = Buffer.from(str, 'utf8')
  const copyLen = Math.min(encoded.length, maxLen - 1)
  encoded.copy(buf, offset, 0, copyLen)
  buf[offset + copyLen] = 0 // null terminator
}
