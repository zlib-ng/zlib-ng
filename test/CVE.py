import sys
import subprocess

def main():
    cmd = sys.argv[1:]
    proc = subprocess.Popen(cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        shell=True)
    stdout, stderr = proc.communicate()
    print stdout
	# We expect 1 error code is OK and we expect 134 or 
    # similar exit code for a vulnerable failure
    if proc.returncode == 1:
        print('zlib is not vulnerable')
        exit(0)
    else:
        print('zlib VULNERABLE')
        exit(proc.returncode)

if __name__ == '__main__':
    main()