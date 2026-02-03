#!/usr/bin/env python3
"""
SeRTial Common Library

Shared data structures and utilities for schema inspection tools.
Uses the same rich data structures as the GUI for consistency.
"""

import json
from dataclasses import dataclass
from typing import List, Optional, Dict, Any
from pathlib import Path


@dataclass
class FieldInfo:
    """Information about a struct field."""
    name: str
    type: str
    index: int
    offset: int
    size: int
    packed_offset: int
    padding_before: int
    # Variable-length container info
    is_variable_length: bool = False
    element_size: int = 0
    max_elements: int = 0
    header_size: int = 0


@dataclass
class BlockInfo:
    """Information about a serialization block."""
    type: str  # "Fixed", "Padding", "Dynamic", "RuntimeOffset"
    src_offset: int
    dst_offset: int
    size: int
    field_index: int
    field_start: int
    field_count: int
    is_variable: bool


@dataclass
class MemcpyRegion:
    """Optimized memcpy region."""
    src_offset: int
    dst_offset: int
    size: int
    field_start: int
    field_count: int


@dataclass
class MessageSchema:
    """Complete schema for a message type."""
    name: str
    category: str
    sizeof_bytes: int
    packed_size: int
    padding_bytes: int
    field_count: int
    has_padding: bool
    can_single_memcpy: bool
    memcpy_region_count: int
    fields: List[FieldInfo]
    memcpy_regions: List[MemcpyRegion]
    # Hybrid memory map data
    has_variable_fields: bool = False
    base_packed_size: int = 0
    fixed_block_count: int = 0
    dynamic_block_count: int = 0
    runtime_offset_block_count: int = 0
    blocks: List[BlockInfo] = None
    
    def __post_init__(self):
        if self.blocks is None:
            self.blocks = []


class SchemaLoader:
    """Load and parse schema JSON files."""
    
    @staticmethod
    def load_file(filepath: str) -> Dict[str, Any]:
        """Load schemas from JSON file."""
        path = Path(filepath)
        if not path.exists():
            raise FileNotFoundError(f"Schema file not found: {filepath}")
        
        with open(filepath, 'r') as f:
            return json.load(f)
    
    @staticmethod
    def parse_message(data: dict) -> MessageSchema:
        """Parse a complete message schema from JSON data."""
        # Get field names and types as separate arrays
        field_names = data.get('field_names', [])
        field_types = data.get('field_types', [])
        field_infos = data.get('field_info', [])
        
        # Combine into FieldInfo objects
        fields = []
        for i, info in enumerate(field_infos):
            name = field_names[i] if i < len(field_names) else f"field_{i}"
            ftype = field_types[i] if i < len(field_types) else "unknown"
            fields.append(FieldInfo(
                name=name,
                type=ftype,
                index=info.get('index', i),
                offset=info.get('offset', 0),
                size=info.get('size', 0),
                packed_offset=info.get('packed_offset', 0),
                padding_before=info.get('padding_before', 0),
                is_variable_length=info.get('is_variable_length', False),
                element_size=info.get('element_size', 0),
                max_elements=info.get('max_elements', 0),
                header_size=info.get('header_size', 0)
            ))
        
        # Parse memcpy regions
        memcpy_regions = [
            MemcpyRegion(
                src_offset=r.get('src_offset', 0),
                dst_offset=r.get('dst_offset', 0),
                size=r.get('size', 0),
                field_start=r.get('field_start', 0),
                field_count=r.get('field_count', 0)
            )
            for r in data.get('memcpy_regions', [])
        ]
        
        # Parse blocks
        blocks = [
            BlockInfo(
                type=b.get('type', 'Fixed'),
                src_offset=b.get('src_offset', 0),
                dst_offset=b.get('dst_offset', 0),
                size=b.get('size', 0),
                field_index=b.get('field_index', -1),
                field_start=b.get('field_start', -1),
                field_count=b.get('field_count', 0),
                is_variable=b.get('is_variable', False)
            )
            for b in data.get('blocks', [])
        ]
        
        return MessageSchema(
            name=data.get('name', ''),
            category=data.get('category', ''),
            sizeof_bytes=data.get('sizeof_bytes', 0),
            packed_size=data.get('packed_size', 0),
            padding_bytes=data.get('padding_bytes', 0),
            field_count=data.get('field_count', 0),
            has_padding=data.get('has_padding', False),
            can_single_memcpy=data.get('can_single_memcpy', False),
            memcpy_region_count=data.get('memcpy_region_count', 0),
            fields=fields,
            memcpy_regions=memcpy_regions,
            has_variable_fields=data.get('has_variable_fields', False),
            base_packed_size=data.get('base_packed_size', data.get('packed_size', 0)),
            fixed_block_count=data.get('fixed_block_count', 0),
            dynamic_block_count=data.get('dynamic_block_count', 0),
            runtime_offset_block_count=data.get('runtime_offset_block_count', 0),
            blocks=blocks if blocks else []
        )
    
    @classmethod
    def load_all_messages(cls, filepath: str) -> List[MessageSchema]:
        """Load all messages from a schema file."""
        data = cls.load_file(filepath)
        messages = []
        
        for msg_data in data.get('messages', []):
            try:
                messages.append(cls.parse_message(msg_data))
            except Exception as e:
                print(f"Warning: Failed to parse message: {e}")
                continue
        
        return messages


class SchemaAnalyzer:
    """Analyze and compute statistics for schemas."""
    
    @staticmethod
    def compute_padding_eliminated(messages: List[MessageSchema]) -> int:
        """Compute total padding bytes eliminated across all messages."""
        return sum(msg.padding_bytes for msg in messages)
    
    @staticmethod
    def count_single_memcpy(messages: List[MessageSchema]) -> int:
        """Count messages that can be serialized with a single memcpy."""
        return sum(1 for msg in messages if msg.can_single_memcpy)
    
    @staticmethod
    def count_variable_messages(messages: List[MessageSchema]) -> int:
        """Count messages with variable-size fields."""
        return sum(1 for msg in messages if msg.has_variable_fields)
    
    @staticmethod
    def average_blocks(messages: List[MessageSchema]) -> float:
        """Compute average number of blocks per message."""
        if not messages:
            return 0.0
        total_blocks = sum(
            msg.fixed_block_count + msg.dynamic_block_count + msg.runtime_offset_block_count
            for msg in messages
        )
        return total_blocks / len(messages)
    
    @staticmethod
    def get_flags(msg: MessageSchema) -> str:
        """Get compact flag representation for a message."""
        flags = []
        if msg.can_single_memcpy:
            flags.append('[1CPY]')
        if msg.has_padding:
            flags.append('[PAD]')
        if msg.has_variable_fields:
            flags.append('[VAR]')
        return ''.join(flags) if flags else ''
