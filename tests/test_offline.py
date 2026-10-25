import json
import math
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time
import unittest
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


class QtServerIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        server = ROOT / "build/server/fau-training-server"
        if not server.is_file():
            raise RuntimeError("build the Qt server before running integration tests")
        probe = socket.socket()
        probe.bind(("127.0.0.1", 0))
        cls.port = probe.getsockname()[1]
        probe.close()
        cls.temp = tempfile.TemporaryDirectory(prefix="fau-training-test-")
        cls.process = subprocess.Popen([str(server), "--port", str(cls.port), "--root", cls.temp.name],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.time() + 5
        while time.time() < deadline:
            try:
                with socket.create_connection(("127.0.0.1", cls.port), timeout=0.2):
                    break
            except OSError:
                if cls.process.poll() is not None:
                    raise RuntimeError("Qt server exited during startup")
                time.sleep(0.05)
        else:
            raise RuntimeError("Qt server did not become ready")

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate()
        try:
            cls.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            cls.process.kill()
            cls.process.wait(timeout=3)
        cls.temp.cleanup()

    def setUp(self):
        self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=5)
        self.reader = self.sock.makefile("rb")
        self.counter = 0
        self.pending = []

    def tearDown(self):
        try:
            self.reader.close()
            self.sock.close()
        except OSError:
            pass

    def receive(self, timeout=8):
        self.sock.settimeout(timeout)
        line = self.reader.readline()
        if not line:
            raise AssertionError("server closed the connection")
        return json.loads(line)

    def command(self, name, payload=None, request_id=None):
        self.counter += 1
        request_id = request_id or f"test-{self.counter}"
        message = {"protocol_version": 1, "request_id": request_id, "type": "command",
                   "command": name, "payload": payload or {}, "timestamp": time.time()}
        self.sock.sendall(json.dumps(message).encode() + b"\n")
        while True:
            reply = self.receive()
            if reply.get("type") == "response" and reply.get("request_id") == request_id:
                return reply
            self.pending.append(reply)

    def event(self, name, timeout=10):
        end = time.time() + timeout
        while time.time() < end:
            if self.pending:
                message = self.pending.pop(0)
            else:
                message = self.receive(max(0.1, end - time.time()))
            if message.get("type") == "event" and message.get("event") == name:
                return message
        self.fail(f"event {name} did not arrive")

    def create_dataset(self, kind, dataset):
        reply = self.command("DATASET_CREATE", {"kind": kind, "dataset": dataset, "count": 20})
        self.assertTrue(reply["ok"], reply)
        return reply

    def wait_until_idle(self):
        for _ in range(80):
            if self.command("GET_STATUS")["payload"]["training_state"] == "IDLE":
                return
            time.sleep(0.05)
        self.fail("worker did not return to IDLE")

    def test_protocol_framing_camera_and_duplicate(self):
        command = {"protocol_version": 1, "request_id": "partial-frame", "type": "command", "command": "HELLO", "payload": {}}
        raw = json.dumps(command).encode()
        self.sock.sendall(raw[:17])
        time.sleep(0.05)
        self.sock.sendall(raw[17:] + b"\n")
        reply = self.receive()
        self.assertEqual(reply["event"], "HELLO")
        self.assertTrue(reply["ok"])
        self.sock.sendall(raw + b"\n")
        duplicate = self.receive()
        self.assertFalse(duplicate["ok"])
        self.assertIn("duplicate", duplicate["error"]["message"])
        self.assertTrue(self.command("CAMERA_LIST")["ok"])
        self.assertTrue(self.command("CAMERA_START", {"role": "top"})["ok"])
        frame = self.event("CAMERA_FRAME")
        encoded = frame["payload"]["image_base64"]
        self.assertGreater(len(encoded), 100)
        self.assertEqual(frame["payload"]["format"], "PNG")
        closed = self.command("CAMERA_CLOSE", {"role":"top"})
        self.assertTrue(closed["ok"], closed)
        self.assertEqual(closed["payload"]["state"], "closed")

    def test_malformed_json_and_incomplete_line(self):
        self.sock.sendall(b'{"protocol_version":1\n')
        malformed = self.receive()
        self.assertFalse(malformed["ok"])
        self.sock.sendall(b'{"protocol_version":1,"request_id":"wait"')
        self.sock.settimeout(0.15)
        with self.assertRaises(socket.timeout):
            self.reader.readline()

    def test_focus_dataset_label_score_and_training_result(self):
        dataset = "focus_" + uuid.uuid4().hex[:8]
        self.create_dataset("focus", dataset)
        self.assertTrue(self.command("DATASET_LABEL", {"kind":"focus", "dataset":dataset,
                         "image":"sample_0000.pgm", "label":"sharp"})["ok"])
        images = self.command("DATASET_IMAGES", {"kind":"focus", "dataset":dataset,"split":"train"})
        self.assertTrue(images["ok"], images)
        self.assertIn("sample_0000.pgm", [row["image"] for row in images["payload"]["images"]])
        missing = self.command("FOCUS_INFER", {"dataset":dataset,"image":"sample_0000.pgm","checkpoint":"missing.json"})
        self.assertFalse(missing["ok"])
        self.assertIn("checkpoint", missing["error"]["message"])
        self.assertTrue(self.command("FOCUS_SCORE", {"dataset":dataset,"image":"sample_0000.pgm"})["ok"])
        start = self.command("FOCUS_TRAIN_START", {"dataset":dataset,"epochs":4,"learning_rate":0.05})
        self.assertTrue(start["ok"], start)
        progress = self.event("TRAINING_PROGRESS")
        self.assertGreaterEqual(progress["payload"]["epoch"], 1)
        completed = self.event("TRAINING_COMPLETED", timeout=15)
        checkpoint = Path(completed["payload"]["checkpoint"])
        self.assertTrue(checkpoint.is_file())
        checkpoints = self.command("MODEL_CHECKPOINTS", {"kind":"focus"})
        self.assertTrue(checkpoints["ok"], checkpoints)
        self.assertIn(checkpoint.name, checkpoints["payload"]["checkpoints"])
        result = json.loads((checkpoint.parent.parent.parent / "runs" / (start["payload"]["run_id"]) / "result.json").read_text())
        self.assertEqual(result["status"], "COMPLETED")
        evaluated = self.command("FOCUS_EVALUATE", {"run_id":start["payload"]["run_id"]})
        self.assertTrue(evaluated["ok"], evaluated)
        inferred = self.command("FOCUS_INFER", {"dataset":dataset,"image":"sample_0000.pgm","checkpoint":checkpoint.name})
        self.assertTrue(inferred["ok"], inferred)
        self.assertIn(inferred["payload"]["label"], ("sharp", "blurred"))
        self.assertGreater(len(inferred["payload"]["image_base64"]), 100)

    def test_fau_annotation_and_baseline_run(self):
        dataset = "fau_" + uuid.uuid4().hex[:8]
        self.create_dataset("fau", dataset)
        capture = self.command("CAMERA_CAPTURE", {"kind":"fau","dataset":dataset,"role":"side"})
        self.assertTrue(capture["ok"], capture)
        self.assertEqual(capture["payload"]["role"], "side")
        annotation = self.command("FAU_ANNOTATION_SAVE", {"dataset":dataset,"image":"sample_0000.pgm","role":"top",
            "roi":{"x":0,"y":0,"width":160,"height":120},"features":[{"id":"TOP_DX","x":80,"y":60,"valid":True,"confidence":0.9}]})
        self.assertTrue(annotation["ok"], annotation)
        invalid = self.command("FAU_ANNOTATION_SAVE", {"dataset":dataset,"image":"sample_0000.pgm","role":"top",
            "roi":{"x":0,"y":0,"width":160,"height":120},"features":[{"id":"TOP_DX","x":500,"y":60}]})
        self.assertFalse(invalid["ok"])
        start = self.command("FAU_TRAIN_START", {"dataset":dataset,"epochs":2})
        self.assertTrue(start["ok"], start)
        complete = self.event("TRAINING_COMPLETED")
        self.assertIn("deterministic", complete["payload"]["warning"])
        evaluated = self.command("FAU_EVALUATE", {"run_id":start["payload"]["run_id"]})
        self.assertTrue(evaluated["ok"], evaluated)
        checkpoint = Path(complete["payload"]["checkpoint"])
        checkpoints = self.command("MODEL_CHECKPOINTS", {"kind":"fau"})
        self.assertTrue(checkpoints["ok"], checkpoints)
        self.assertIn(checkpoint.name, checkpoints["payload"]["checkpoints"])
        inferred = self.command("FAU_INFER", {"dataset":dataset,"image":"sample_0000.pgm","role":"top","checkpoint":checkpoint.name})
        self.assertTrue(inferred["ok"], inferred)
        self.assertTrue(inferred["payload"]["predictions"])
        self.assertIn("calibration", inferred["payload"])

    def test_pretrained_worker_reports_missing_dependency(self):
        if __import__("importlib.util", fromlist=["find_spec"]).find_spec("torch"):
            self.skipTest("PyTorch is installed; do not launch a pretrained-weight download in this offline test")
        dataset = "pretrained_" + uuid.uuid4().hex[:8]
        self.create_dataset("focus", dataset)
        start = self.command("FOCUS_TRAIN_START", {"dataset":dataset,"architecture":"mobilenet_v3_small",
                              "epochs":1,"batch_size":2,"learning_rate":0.0001,"device":"cpu"})
        self.assertTrue(start["ok"], start)
        failed = self.event("TRAINING_FAILED", timeout=10)
        self.assertIn("PyTorch", failed["payload"]["error"])

    def test_dataset_folder_import_and_split(self):
        dataset = "import_" + uuid.uuid4().hex[:8]
        self.create_dataset("focus", dataset)
        source = Path(self.temp.name) / "external-images"
        source.mkdir(exist_ok=True)
        pixels = bytes([0, 255] * 8)
        (source / "extra.pgm").write_bytes(b"P5\n4 4\n255\n" + pixels)
        imported = self.command("DATASET_IMPORT", {"kind":"focus","dataset":dataset,"path":str(source)})
        self.assertTrue(imported["ok"], imported)
        self.assertEqual(imported["payload"]["imported"], 1)
        rows = [json.loads(line) for line in (Path(self.temp.name) / "data/focus" / dataset / "manifest.jsonl").read_text().splitlines()]
        imported_row = next(row for row in rows if row.get("source_name") == "extra.pgm")
        split = self.command("DATASET_SPLIT", {"kind":"focus","dataset":dataset,"image":imported_row["image"],"split":"test"})
        self.assertTrue(split["ok"], split)
        test_images = self.command("DATASET_IMAGES", {"kind":"focus","dataset":dataset,"split":"test"})
        self.assertTrue(test_images["ok"], test_images)
        self.assertEqual([row["image"] for row in test_images["payload"]["images"]], [imported_row["image"]])

    def test_cancel_and_reconnect_active_job(self):
        dataset = "cancel_" + uuid.uuid4().hex[:8]
        self.create_dataset("focus", dataset)
        start = self.command("FOCUS_TRAIN_START", {"dataset":dataset,"epochs":200})
        self.assertTrue(start["ok"], start)
        self.command("FOCUS_TRAIN_CANCEL")
        self.event("TRAINING_CANCELLED", timeout=8)
        self.wait_until_idle()
        for _ in range(50):
            if self.command("GET_STATUS")["payload"]["training_state"] == "IDLE":
                break
            time.sleep(0.05)
        else:
            self.fail("cancelled worker did not return to IDLE")
        running = self.command("FOCUS_TRAIN_START", {"dataset":dataset,"epochs":100})
        self.assertTrue(running["ok"], running)
        self.reader.close()
        self.sock.close()
        self.pending.clear()
        self.sock = socket.create_connection(("127.0.0.1", self.port), timeout=5)
        self.reader = self.sock.makefile("rb")
        status = self.command("GET_STATUS")
        self.assertEqual(status["payload"]["training_state"], "RUNNING")
        self.assertEqual(status["payload"]["active_run_id"], running["payload"]["run_id"])
        self.command("FOCUS_TRAIN_CANCEL")
        deadline = time.time() + 8
        while time.time() < deadline:
            message = self.pending.pop(0) if self.pending else self.receive(deadline - time.time())
            if message.get("event") == "TRAINING_CANCELLED" and message.get("payload", {}).get("run_id") == running["payload"]["run_id"]:
                break
        else:
            self.fail("reconnected client did not receive cancellation for the active run")
        self.wait_until_idle()

    def test_motion_limits_fail_closed_and_simulate_only(self):
        limits = self.command("MOTION_LIMIT_GET")["payload"]
        self.assertTrue(limits["simulation_only"])
        feedback = self.command("MOTION_FEEDBACK_GET")["payload"]
        self.assertEqual(feedback["pose_source"], "SIMULATED")
        self.assertEqual(feedback["raw_count_source"], "UNAVAILABLE")
        self.assertFalse(feedback["raw_counts_available"])
        bad = json.loads(json.dumps(limits))
        bad["axes"]["X"]["min"] = bad["axes"]["X"]["max"]
        self.assertFalse(self.command("MOTION_LIMIT_VALIDATE", {"limits":bad})["ok"])
        request = self.command("MOTION_SIMULATE", {"axis":"X","target":1.0,"speed":0.1})
        self.assertFalse(request["ok"])
        accepted = self.command("MOTION_SIMULATE", {"axis":"X","target":0.1,"speed":0.1})
        self.assertTrue(accepted["ok"], accepted)
        self.assertTrue(accepted["payload"]["simulation_only"])
        self.assertEqual(accepted["payload"]["motion_feedback"]["pose"][0], 0.1)

    def test_physical_backends_are_unavailable_and_commands_fail_closed(self):
        status = self.command("MOTION_BACKEND_STATUS")["payload"]
        self.assertEqual(status["backend"], "simulated")
        self.assertFalse(status["physical_control_enabled"])
        backends = self.command("MOTION_BACKEND_LIST")["payload"]["backends"]
        self.assertEqual({row["id"]: row["availability"] for row in backends},
                         {"simulated": "SIMULATED", "nec": "UNAVAILABLE", "gmrt": "UNAVAILABLE"})
        for command, payload in (("MOTION_SERVO_ON", {}), ("MOTION_HOME", {}),
                                 ("MOTION_MOVEL", {"pose": [0, 0, 0, 0, 0, 0]})):
            reply = self.command(command, payload)
            self.assertFalse(reply["ok"], reply)
            self.assertIn("SIMULATED", reply["payload"]["availability"])
        unavailable = self.command("MOTION_BACKEND_SELECT", {"backend": "nec"})
        self.assertFalse(unavailable["ok"])

    def test_camera_roles_share_server_owned_service(self):
        cameras = self.command("CAMERA_LIST")["payload"]
        self.assertEqual(cameras["ownership"], "server-shared-service")
        self.assertEqual(cameras["backend"], "synthetic")
        self.assertEqual({row["role"] for row in cameras["cameras"]}, {"top", "side"})
        self.assertTrue(all(row["state"] == "SIMULATED" for row in cameras["cameras"]))
        status = self.command("GET_STATUS")["payload"]
        self.assertEqual(status["camera_backend"], "synthetic")
        self.assertEqual(status["camera_status"], "SIMULATED")

    def test_mock_runtime_state_transitions_do_not_load_models(self):
        status = self.command("MODEL_RUNTIME_STATUS")["payload"]
        self.assertEqual(status["mode"], "mock-only")
        self.assertEqual(status["focus"]["state"], "UNLOADED")
        loaded = self.command("MODEL_RUNTIME_MOCK_LOAD", {"kind": "focus"})["payload"]
        self.assertEqual(loaded["focus"]["state"], "MOCK_LOADED")
        self.assertFalse(loaded["focus"]["checkpoint_read"])
        self.assertFalse(loaded["focus"]["inference_enabled"])
        self.assertEqual(loaded["fau"]["state"], "UNLOADED")
        unloaded = self.command("MODEL_RUNTIME_MOCK_UNLOAD", {"kind": "focus"})["payload"]
        self.assertEqual(unloaded["focus"]["state"], "UNLOADED")
        self.assertFalse(self.command("MODEL_RUNTIME_MOCK_LOAD", {"kind": "other"})["ok"])

    def test_widgets_application_starts_offscreen(self):
        env = os.environ.copy()
        env["QT_QPA_PLATFORM"] = "offscreen"
        process = subprocess.Popen([str(ROOT / "build/ui/fau-training-ui")], env=env,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.4)
        self.assertIsNone(process.poll(), "Qt Widgets application exited during startup")
        process.terminate()
        process.wait(timeout=3)

    def test_ui_exposes_model_testing_and_status_controls(self):
        tree = ET.parse(ROOT / "UI/qt-ui/mainwindow.ui")
        names = {widget.get("name") for widget in tree.iter("widget")}
        required = {"focusTestSplitCombo", "focusTestImageCombo", "loadFocusTestImagesButton",
                    "focusCheckpointCombo", "loadFocusCheckpointsButton",
                    "captureFauImageButton",
                    "fauTestSplitCombo", "fauTestImageCombo", "fauTestRoleCombo",
                    "loadFauTestImagesButton", "fauCheckpointCombo", "loadFauCheckpointsButton",
                    "focusInferenceCheckpointEdit",
                    "fauInferenceCheckpointEdit", "serverHealthLabel", "futureGmrtLabel",
                    "poseValidityLabel", "poseTargetLabel", "countSourceLabel",
                    "focusTestDatasetEdit", "focusTestPageImageCombo",
                    "focusTestPageCheckpointCombo", "fauTestPageImageCombo",
                    "fauTestPageCheckpointCombo", "runtimeFocusStateLabel",
                    "runtimeFauStateLabel", "runtimeInferenceStateLabel",
                    "runtimeCameraCombo", "runtimeEnableInferenceButton",
                    "runtimeDisableInferenceButton", "runtimeRoiStatusLabel",
                    "machineInitializeButton", "servoOnButton", "homeButton", "moveLButton"}
        self.assertTrue(required.issubset(names), f"missing UI controls: {required - names}")
        titles = {node.text for node in tree.iter("string") if node.text}
        self.assertTrue({"Motion Control", "Cameras and Dataset", "Focus Training", "FAU Training",
                         "Model Testing", "Model Runtime", "Settings and Logs"}.issubset(titles))
        for name in ("machineInitializeButton", "servoOnButton", "homeButton", "moveLButton",
                     "runtimeEnableInferenceButton", "runtimeDisableInferenceButton"):
            widget = next(node for node in tree.iter("widget") if node.get("name") == name)
            enabled = widget.find("./property[@name='enabled']/bool")
            self.assertIsNotNone(enabled, name)
            self.assertEqual(enabled.text, "false", name)

    def test_reference_build_helper_rejects_in_repository_source(self):
        process = subprocess.run(["python3", str(ROOT / "scripts/build_original_qt_reference.py"),
                                  "--project-file", str(ROOT / "UI/qt-ui/qt-ui.pro")],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(process.returncode, 2)
        self.assertIn("must remain outside the Git repository", process.stderr)

    def test_reference_build_helper_rejects_in_tree_output(self):
        external = Path(self.temp.name) / "reference"
        external.mkdir(exist_ok=True)
        project = external / "qt-ui.pro"
        project.write_text("TEMPLATE = app\n")
        process = subprocess.run(["python3", str(ROOT / "scripts/build_original_qt_reference.py"),
                                  "--project-file", str(project), "--build-dir", str(external / "build")],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        self.assertEqual(process.returncode, 2)
        self.assertIn("outside the read-only reference project", process.stderr)


if __name__ == "__main__":
    unittest.main()
