from pathlib import Path
import json
Import("env")
source = Path(env.subst("$PROJECT_DIR")) / "firmware_data/model.json"
model = json.loads(source.read_text(encoding="utf-8"))
payload = json.dumps(model, separators=(",", ":"))
out = Path(env.subst("$BUILD_DIR")) / "generated"
out.mkdir(parents=True, exist_ok=True)
(out / "test_model.h").write_text(
    '#pragma once\nstatic const char TEST_MODEL[] = R"MODEL(' + payload + ')MODEL";\n',
    encoding="utf-8")
env.Append(CPPPATH=[str(out)])
