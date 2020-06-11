from cx_Freeze import setup, Executable

base = None

executables = [Executable("esp_client.py", base=base)]

packages = ["idna","socket","sys"]
options = {
    'build_exe': {
        'packages':packages,
    },
}

setup(
    name = "<first ever>",
    options = options,
    version = "0.11",
    description = '<test>',
    executables = executables, requires=['bitstring']
)