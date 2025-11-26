
def is_in_user_namespace():
    try:
        with open("/proc/self/uid_map", "r") as f:
            uid_map = f.read().strip()
    # uid_map format: InsideUID   OutsideUID   Length
    # If we're in a user namespace, InsideUID 0 might map to a non-zero OutsideUID
        for line in uid_map.splitlines():
            inside_uid, outside_uid, length = map(int, line.split())
            if inside_uid == 0 and outside_uid != 0:
                return True
            if inside_uid != 0 and outside_uid != 0:
                return True
            return False
    except Exception as e:
        print(f"Error reading UID map: {e}")
    return False


if __name__ == "__main__":
    if is_in_user_namespace():
        print("Running inside a sandboxed environment.")
    else:
        print("Running outside the sandboxed environment.")

