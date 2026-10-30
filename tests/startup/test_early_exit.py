"""Run the actual entry point, including the Harmony resource diagnostic block."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    app = args.app.resolve()
    results = []
    with tempfile.TemporaryDirectory(prefix='leocad-startup-') as temporary:
        env = os.environ.copy()
        env.update(QT_QPA_PLATFORM='offscreen', QT_FORCE_STDERR_LOGGING='1', LC_ALL='C', LANG='C',
                   XDG_CONFIG_HOME=temporary, XDG_DATA_HOME=temporary,
                   XDG_CACHE_HOME=temporary, TEMP=temporary, TMP=temporary)
        for option, expected_exit, expected_text in [
            ('--version', 0, ('LeoCAD', '26.09')),
            ('--help', 0, ('leocad', '--version', '--help')),
            ('--lc-invalid-option-for-regression', 1, ()),
        ]:
            result = dict(option=option, expected_exit=expected_exit, exit_code=None,
                          stdout='', stderr='', reports=[], passed=False)
            try:
                completed = subprocess.run([str(app), option], cwd=temporary, env=env,
                                           stdin=subprocess.DEVNULL, capture_output=True,
                                           timeout=15)
                result.update(exit_code=completed.returncode,
                              stdout=completed.stdout.decode('utf-8', errors='replace'),
                              stderr=completed.stderr.decode('utf-8', errors='replace'))
                for line in result['stderr'].splitlines():
                    if 'LEOCAD_RESOURCE_PATHS ' in line:
                        result['reports'].append(json.loads(line.split('LEOCAD_RESOURCE_PATHS ', 1)[1]))
                errors = []
                if completed.returncode != expected_exit:
                    errors.append(f'exit {completed.returncode}, expected {expected_exit}')
                if any(text not in result['stdout'] for text in expected_text):
                    errors.append('expected help/version output missing')
                if expected_exit == 0:
                    # Requiring the report prevents a normal desktop build with
                    # the Harmony-only block excluded from falsely passing.
                    if len(result['reports']) != 1:
                        errors.append('expected exactly one resource report')
                    elif ('loaded_parts' not in result['reports'][0]
                          or result['reports'][0]['loaded_parts'] is not None):
                        errors.append('uninitialized library must report null, not a piece count')
                else:
                    if option not in result['stderr']:
                        errors.append('unknown option error missing')
                    if result['reports']:
                        errors.append('error startup must return before the resource report')
                result['errors'] = errors
                result['passed'] = not errors
            except (subprocess.TimeoutExpired, OSError, ValueError) as error:
                result['errors'] = [str(error)]
            results.append(result)
            print(f'{"PASS" if result["passed"] else "FAIL"}: {option}, exit={result["exit_code"]}')

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(dict(app=str(app), results=results), indent=2) + '\n', encoding='utf-8')
    return 0 if all(result['passed'] for result in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
