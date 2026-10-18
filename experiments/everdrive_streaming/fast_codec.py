"""Diagnostic import of the alternative native-friendly storage codec."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from pose_codec_fast import normalize,mapping,encode,decode
