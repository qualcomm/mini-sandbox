# Build wheel

Execute from the current directory

```python
python -m venv .venv_test
source .venv_test/bin/activate
python -m pip install --upgrade pip setuptools wheel build
python -m build

# Verify that the wheel includes everything
python check_package.py
```

## Install the wheel

You can either install in the current virtual env, generate a new virtual env (see first two commands in the above snippet) or install system-wide. The only needed command is

```python
pip install dist/*.whl
```

Now you should be able to import `pyminisandbox`
