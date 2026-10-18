"""Diagnostic imports the same independently verified storage format as the packer."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools'))
from pose_codec import normalize, mapping, encode, decode
