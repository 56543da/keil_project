import os

root_dir = r"e:\keil_project"
extensions = ['.c', '.h']

def convert_file(file_path):
    # Try reading as UTF-8 first
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        # It's already valid UTF-8
        return "UTF-8 (Skipped)"
    except UnicodeDecodeError:
        pass
    
    # Try reading as GBK
    try:
        with open(file_path, 'r', encoding='gbk') as f:
            content = f.read()
        
        # Write back as UTF-8
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        return "Converted to UTF-8"
    except UnicodeDecodeError:
        return "Unknown Encoding (Skipped)"
    except Exception as e:
        return f"Error: {e}"

print(f"Scanning {root_dir}...")
count = 0
for dirpath, dirnames, filenames in os.walk(root_dir):
    # Skip hidden folders like .git or .vscode
    if any(p.startswith('.') for p in dirpath.split(os.sep)):
        continue
        
    for filename in filenames:
        if any(filename.lower().endswith(ext) for ext in extensions):
            file_path = os.path.join(dirpath, filename)
            result = convert_file(file_path)
            if "Converted" in result:
                print(f"{file_path}: {result}")
                count += 1

print(f"Done. Converted {count} files.")
