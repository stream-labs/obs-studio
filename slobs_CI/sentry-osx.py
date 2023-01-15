import os
os.system('curl -sL https://sentry.io/get-cli/ | bash')

def process_sentry(directory):
    for root, dirs, files in os.walk(directory):
        for file in files:
            if 'lib' in file or 'obs' in file or '.so' in file or '.dylib' in file:
                path = os.path.join(root, file)
                print("Running sentry-cli on '" + file + "' to obs-server and obs-server-preview, value = " + root + ".dSYM/Contents/Resources/DWARF/" + file)
                os.system("dsymutil " + path)
                os.system("sentry-cli --auth-token ${SENTRY_AUTH_TOKEN} upload-dif --org streamlabs-desktop --project obs-server " + root + ".dSYM/Contents/Resources/DWARF/" + file)                
                os.system("dsymutil " + path)
                os.system("sentry-cli --auth-token ${SENTRY_AUTH_TOKEN} upload-dif --org streamlabs-desktop --project obs-server-preview " + root + ".dSYM/Contents/Resources/DWARF/" + file)

# Upload obs debug files
process_sentry(os.path.join(os.environ['PWD'], 'build'))