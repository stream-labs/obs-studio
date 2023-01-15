import os
os.system('curl -sL https://sentry.io/get-cli/ | bash')

def process_sentry(directory):
    print("1. process_sentry directory is...")
    print(directory)
    for root, dirs, files in os.walk(rootFolderPath):
        for file in files:
            print("2. filepath/root is...")
            print(os.path.join(root, file))
            print(root)
            if 'lib' in file or 'obs' in file or '.so' in file or '.dylib' in file:
                path = os.path.join(root, file)
                print("3. uploading...")
                print(path)

# Upload obs debug files
process_sentry(os.path.join(os.environ['PWD'], 'build'))