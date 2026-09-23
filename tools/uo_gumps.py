"""Decode and build the variable-length packets used by classic UO gumps.

The test client only needs the semantic part of a gump: its identity, the
layout controls, the text table, and the button ids.  Keeping this parser
separate from stream framing makes it usable with either a complete packet or
one packet returned by :func:`uo_packets.split_packet_stream`.
"""

from __future__ import annotations

from dataclasses import dataclass
import re
import struct
from typing import Iterable, Optional, Sequence
import zlib


class GumpPacketError(ValueError):
    """Raised when a gump packet is malformed or incomplete."""


@dataclass(frozen=True)
class GumpControl:
    """One layout control and its string arguments."""

    kind: str
    args: tuple[str, ...]

    @property
    def x(self) -> Optional[int]:
        return _int_arg(self.args, 0)

    @property
    def y(self) -> Optional[int]:
        return _int_arg(self.args, 1)


@dataclass(frozen=True)
class GumpButton:
    """A button control extracted from a gump layout."""

    button_id: int
    page: int
    x: int
    y: int
    normal_graphic: int
    pressed_graphic: int
    controls_index: int


@dataclass(frozen=True)
class GumpDialog:
    """Decoded 0xB0 or 0xDD server gump."""

    command: int
    serial: int
    context: int
    x: int
    y: int
    layout: str
    controls: tuple[GumpControl, ...]
    texts: tuple[str, ...]
    buttons: tuple[GumpButton, ...]

    def find_button(
        self, *, button_id: Optional[int] = None, label: Optional[str] = None
    ) -> Optional[GumpButton]:
        """Find a button by id or by the text control nearest its label.

        Classic gump layouts carry labels in a separate text table.  A label
        is therefore associated with the closest ``text``/``croppedtext``
        control on the same row; an exact text match wins over a substring
        match.  Button ids are always preferred when supplied.
        """

        if button_id is not None:
            return next((b for b in self.buttons if b.button_id == button_id), None)
        if label is None:
            return None

        wanted = label.strip().casefold()
        matches: list[tuple[bool, int, int, int, int]] = []
        for control_index, control in enumerate(self.controls):
            text_index = _text_control_index(control)
            if text_index is None or not 0 <= text_index < len(self.texts):
                continue
            text = self.texts[text_index].strip()
            folded = text.casefold()
            if folded != wanted and wanted not in folded:
                continue
            cx = control.x
            cy = control.y
            if cx is None or cy is None:
                continue
            for button_index, button in enumerate(self.buttons):
                # Prefer a button to the left of its label on the same row.
                same_row = abs(button.y - cy)
                x_distance = abs(cx - button.x)
                left_of_label = button.x <= cx
                matches.append(
                    (
                        folded != wanted,
                        same_row,
                        0 if left_of_label else 1,
                        x_distance,
                        button_index,
                    )
                )
        if not matches:
            return None
        return self.buttons[min(matches)[-1]]


@dataclass(frozen=True)
class GumpTextEntry:
    entry_id: int
    text: str


@dataclass(frozen=True)
class GumpReply:
    """Decoded 0xB1 client reply."""

    serial: int
    context: int
    button_id: int
    switches: tuple[int, ...]
    texts: tuple[GumpTextEntry, ...]


@dataclass(frozen=True)
class SkillUse:
    """Decoded 0x12 extended-command skill request."""

    skill_id: int
    arguments: tuple[str, ...]


def _int_arg(args: Sequence[str], index: int) -> Optional[int]:
    if index >= len(args):
        return None
    try:
        return int(args[index], 0)
    except ValueError:
        return None


def _text_control_index(control: GumpControl) -> Optional[int]:
    if control.kind in {"text", "croppedtext"}:
        return _int_arg(control.args, 3 if control.kind == "text" else 5)
    if control.kind in {"htmlgump", "xmfhtmlgump", "xmfhtmlgumpcolor"}:
        # Html controls carry a cliloc id rather than an entry in the text
        # table.  Treating that id as text would associate unrelated buttons.
        return None
    return None


_CONTROL_RE = re.compile(r"\{([^{}]*)\}")


def _parse_controls(
    layout_bytes: bytes,
) -> tuple[str, tuple[GumpControl, ...], tuple[GumpButton, ...]]:
    try:
        layout = layout_bytes.rstrip(b"\x00").decode("ascii")
    except UnicodeDecodeError as error:
        raise GumpPacketError("gump layout is not ASCII") from error

    controls: list[GumpControl] = []
    buttons: list[GumpButton] = []
    cursor = 0
    for match in _CONTROL_RE.finditer(layout):
        if layout[cursor:match.start()].strip():
            raise GumpPacketError("gump layout contains text outside a control")
        fields = tuple(match.group(1).split())
        if not fields:
            raise GumpPacketError("gump layout contains an empty control")
        control = GumpControl(fields[0].casefold(), fields[1:])
        control_index = len(controls)
        controls.append(control)
        if control.kind == "button" and len(control.args) >= 7:
            x = _int_arg(control.args, 0)
            y = _int_arg(control.args, 1)
            normal = _int_arg(control.args, 2)
            pressed = _int_arg(control.args, 3)
            page = _int_arg(control.args, 5)
            button_id = _int_arg(control.args, 6)
            if None in (x, y, normal, pressed, page, button_id):
                raise GumpPacketError("gump button has a non-numeric field")
            buttons.append(
                GumpButton(
                    button_id=button_id,
                    page=page,
                    x=x,
                    y=y,
                    normal_graphic=normal,
                    pressed_graphic=pressed,
                    controls_index=control_index,
                )
            )
        cursor = match.end()
    if layout[cursor:].strip():
        raise GumpPacketError("gump layout has an unterminated control")
    return layout, tuple(controls), tuple(buttons)


def _decode_text_lines(
    data: bytes, count: int, offset: int, *, end: int
) -> tuple[tuple[str, ...], int]:
    texts: list[str] = []
    for _ in range(count):
        if offset + 2 > end:
            raise GumpPacketError("truncated gump text length")
        length = int.from_bytes(data[offset:offset + 2], "big")
        offset += 2
        byte_length = length * 2
        if offset + byte_length > end:
            raise GumpPacketError("truncated gump unicode text")
        try:
            texts.append(data[offset:offset + byte_length].decode("utf-16-be"))
        except UnicodeDecodeError as error:
            raise GumpPacketError("invalid gump unicode text") from error
        offset += byte_length
    return tuple(texts), offset


def _finish_dialog(
    command: int,
    serial: int,
    context: int,
    x: int,
    y: int,
    layout_bytes: bytes,
    texts: tuple[str, ...],
) -> GumpDialog:
    layout, controls, buttons = _parse_controls(layout_bytes)
    return GumpDialog(command, serial, context, x, y, layout, controls, texts, buttons)


def _check_declared_length(data: bytes) -> int:
    if len(data) < 3:
        raise GumpPacketError("truncated gump packet header")
    declared = int.from_bytes(data[1:3], "big")
    if declared != len(data):
        raise GumpPacketError(
            f"gump packet length is {len(data)}, declared {declared}"
        )
    return declared


def _parse_b0(data: bytes) -> GumpDialog:
    _check_declared_length(data)
    if len(data) < 23:
        raise GumpPacketError("truncated 0xB0 gump header")
    serial, context, x, y = struct.unpack_from(">IIII", data, 3)
    command_length = struct.unpack_from(">H", data, 19)[0]
    if command_length < 1:
        raise GumpPacketError("0xB0 gump command section is empty")
    command_start = 21
    separator = command_start + command_length - 1
    if separator >= len(data) or data[separator] != 0:
        raise GumpPacketError("0xB0 gump command section is not terminated")
    text_count_offset = command_start + command_length
    if text_count_offset + 2 > len(data):
        raise GumpPacketError("truncated 0xB0 gump text count")
    text_count = struct.unpack_from(">H", data, text_count_offset)[0]
    texts, end = _decode_text_lines(
        data, text_count, text_count_offset + 2, end=len(data)
    )
    if end != len(data):
        raise GumpPacketError("0xB0 gump has trailing bytes")
    return _finish_dialog(
        0xB0,
        serial,
        context,
        x,
        y,
        data[command_start:separator],
        texts,
    )


def _zlib_payload(
    data: bytes, offset: int, compressed_length: int, *, end: int
) -> tuple[bytes, int]:
    if compressed_length < 4:
        raise GumpPacketError("compressed gump length is smaller than its header")
    payload_length = compressed_length - 4
    payload_end = offset + payload_length
    if payload_end > end:
        raise GumpPacketError("truncated compressed gump payload")
    try:
        decoded = zlib.decompress(data[offset:payload_end])
    except zlib.error as error:
        raise GumpPacketError("invalid compressed gump payload") from error
    return decoded, payload_end


def _parse_dd(data: bytes) -> GumpDialog:
    _check_declared_length(data)
    if len(data) < 27:
        raise GumpPacketError("truncated 0xDD gump header")
    serial, context, x, y = struct.unpack_from(">IIII", data, 3)
    compressed_length, decompressed_length = struct.unpack_from(">II", data, 19)
    layout, offset = _zlib_payload(data, 27, compressed_length, end=len(data))
    if len(layout) != decompressed_length:
        raise GumpPacketError("compressed gump layout length mismatch")
    if offset + 4 > len(data):
        raise GumpPacketError("truncated compressed gump text count")
    text_count = struct.unpack_from(">I", data, offset)[0]
    offset += 4
    if offset + 8 > len(data):
        raise GumpPacketError("truncated compressed gump text header")
    compressed_text_length, decompressed_text_length = struct.unpack_from(">II", data, offset)
    offset += 8
    text_bytes, offset = _zlib_payload(
        data, offset, compressed_text_length, end=len(data)
    )
    if len(text_bytes) != decompressed_text_length:
        raise GumpPacketError("compressed gump text length mismatch")
    texts, text_end = _decode_text_lines(
        text_bytes, text_count, 0, end=len(text_bytes)
    )
    if text_end != len(text_bytes) or offset != len(data):
        raise GumpPacketError("compressed gump has trailing bytes")
    return _finish_dialog(0xDD, serial, context, x, y, layout, texts)


def parse_gump_dialog(data: bytes) -> GumpDialog:
    """Decode an uncompressed 0xB0 or zlib-compressed 0xDD gump."""

    if not data:
        raise GumpPacketError("empty gump packet")
    if data[0] == 0xB0:
        return _parse_b0(data)
    if data[0] == 0xDD:
        return _parse_dd(data)
    raise GumpPacketError(f"expected 0xB0 or 0xDD, got 0x{data[0]:02x}")


def _bounded_u32(value: int, name: str) -> int:
    if not isinstance(value, int) or not 0 <= value <= 0xFFFFFFFF:
        raise ValueError(f"{name} must be an unsigned 32-bit integer")
    return value


def make_gump_reply(
    serial: int,
    context: int,
    button_id: int,
    switches: Iterable[int] = (),
    texts: Iterable[tuple[int, str]] = (),
) -> bytes:
    """Build a 0xB1 reply with switches and UTF-16BE text entries."""

    serial = _bounded_u32(serial, "serial")
    context = _bounded_u32(context, "context")
    button_id = _bounded_u32(button_id, "button_id")
    switch_values = tuple(_bounded_u32(value, "switch") for value in switches)
    text_values = tuple(texts)
    body = bytearray(struct.pack(">III", serial, context, button_id))
    body += struct.pack(">I", len(switch_values))
    for value in switch_values:
        body += struct.pack(">I", value)
    body += struct.pack(">I", len(text_values))
    for entry_id, text in text_values:
        if not isinstance(entry_id, int) or not 0 <= entry_id <= 0xFFFF:
            raise ValueError("text entry id must be an unsigned 16-bit integer")
        encoded = str(text).encode("utf-16-be")
        if len(encoded) % 2 or len(encoded) // 2 > 0xFFFF:
            raise ValueError("text entry is too long")
        body += struct.pack(">HH", entry_id, len(encoded) // 2)
        body += encoded
    packet_length = 3 + len(body)
    if packet_length > 0xFFFF:
        raise ValueError("gump reply exceeds the protocol length field")
    return b"\xB1" + struct.pack(">H", packet_length) + bytes(body)


def parse_gump_reply(data: bytes) -> GumpReply:
    """Decode a complete 0xB1 reply packet."""

    if not data or data[0] != 0xB1:
        raise GumpPacketError("expected a 0xB1 gump reply")
    _check_declared_length(data)
    if len(data) < 19:
        raise GumpPacketError("truncated 0xB1 gump reply")
    serial, context, button_id, switch_count = struct.unpack_from(">IIII", data, 3)
    offset = 19
    switches: list[int] = []
    for _ in range(switch_count):
        if offset + 4 > len(data):
            raise GumpPacketError("truncated gump switch list")
        switches.append(struct.unpack_from(">I", data, offset)[0])
        offset += 4
    if offset + 4 > len(data):
        raise GumpPacketError("truncated gump text count")
    text_count = struct.unpack_from(">I", data, offset)[0]
    offset += 4
    texts: list[GumpTextEntry] = []
    for _ in range(text_count):
        if offset + 4 > len(data):
            raise GumpPacketError("truncated gump text entry header")
        entry_id, length = struct.unpack_from(">HH", data, offset)
        offset += 4
        byte_length = length * 2
        if offset + byte_length > len(data):
            raise GumpPacketError("truncated gump text entry")
        try:
            text = data[offset:offset + byte_length].decode("utf-16-be")
        except UnicodeDecodeError as error:
            raise GumpPacketError("invalid gump text entry") from error
        texts.append(GumpTextEntry(entry_id, text))
        offset += byte_length
    if offset != len(data):
        raise GumpPacketError("gump reply has trailing bytes")
    return GumpReply(serial, context, button_id, tuple(switches), tuple(texts))


def make_skill_use(skill_id: int) -> bytes:
    """Build the 0x12/0x24 extended command used to start a skill.

    Sphere's classic client command carries a null-terminated ``"id 0"``
    string.  The second field is retained for wire compatibility; the server
    consumes the first integer as the skill id.
    """

    if not isinstance(skill_id, int) or not 0 <= skill_id <= 0xFFFF:
        raise ValueError("skill_id must be an unsigned 16-bit integer")
    payload = f"{skill_id} 0\0".encode("ascii")
    packet_length = 4 + len(payload)
    if packet_length > 0xFFFF:
        raise ValueError("skill request exceeds the protocol length field")
    return b"\x12" + struct.pack(">H", packet_length) + b"\x24" + payload


def parse_skill_use(data: bytes) -> SkillUse:
    """Decode a complete 0x12/0x24 skill request."""

    if len(data) < 5 or data[0] != 0x12:
        raise GumpPacketError("expected a 0x12 skill request")
    _check_declared_length(data)
    if data[3] != 0x24:
        raise GumpPacketError(f"expected skill subtype 0x24, got 0x{data[3]:02x}")
    payload = data[4:]
    if not payload or payload[-1] != 0:
        raise GumpPacketError("skill request is not NUL terminated")
    try:
        fields = payload[:-1].decode("ascii").split()
    except UnicodeDecodeError as error:
        raise GumpPacketError("skill request is not ASCII") from error
    if not fields:
        raise GumpPacketError("skill request has no skill id")
    try:
        skill_id = int(fields[0], 10)
    except ValueError as error:
        raise GumpPacketError("skill request has a non-numeric skill id") from error
    if not 0 <= skill_id <= 0xFFFF:
        raise GumpPacketError("skill id is outside the supported range")
    return SkillUse(skill_id, tuple(fields[1:]))
