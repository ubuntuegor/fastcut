import subprocess
import json
import os

def run_fastcut(input_file, start_time, end_time, output_file):
  process = subprocess.run(["../build/fastcut", input_file, start_time, end_time, output_file])
  process.check_returncode()

def get_media_data(filename):
  base_command = "ffprobe -show_streams -show_format -count_frames -of json"
  ffprobe_process = subprocess.run(base_command.split(" ") + ["-i", filename], capture_output = True, encoding="utf-8")
  return json.loads(ffprobe_process.stdout)

def video_stream(media_data):
  return next(filter(lambda s: s["codec_type"] == "video", media_data["streams"]))

def test_clock_whole():
  run_fastcut("../samples/clock.mp4", "0", "31", "clock_whole.mp4")
  try:
    original_data = get_media_data("../samples/clock.mp4")
    cut_data = get_media_data("clock_whole.mp4")
    assert video_stream(original_data)["nb_frames"] == video_stream(cut_data)["nb_frames"]
    assert original_data["format"]["duration"] == cut_data["format"]["duration"]
  finally:
    os.remove("clock_whole.mp4")

def test_clock_start():
  run_fastcut("../samples/clock.mp4", "0", "10", "clock_start.mp4")
  try:
    cut_data = get_media_data("clock_start.mp4")
    assert video_stream(cut_data)["nb_frames"] == "300"
    assert cut_data["format"]["duration"].split(".")[0] == "10"
  finally:
    os.remove("clock_start.mp4")

def test_clock_end():
  run_fastcut("../samples/clock.mp4", "20", "31", "clock_end.mp4")
  try:
    cut_data = get_media_data("clock_end.mp4")
    assert video_stream(cut_data)["nb_frames"] == "301"
    assert cut_data["format"]["duration"].split(".")[0] == "10"
  finally:
    os.remove("clock_end.mp4")

test_clock_whole()
test_clock_start()
test_clock_end()
