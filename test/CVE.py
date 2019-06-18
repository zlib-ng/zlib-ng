import sys
import subprocess

def main():
    cmd = sys.argv[2:]
    proc = subprocess.Popen(cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT)
    stdout, stderr = proc.communicate()
    print(stdout)

    # Exit code 0 or 1 is OK
    # Exit code 134 or similar is a vulnerable failure
    if proc.returncode == 0 or proc.returncode == 1:
        print('zlib not vulnerable to {0} ({1})'.format(sys.argv[1], proc.returncode))
        exit(0)
    else:
        print('zlib VULNERABLE to {0} ({1})'.format(sys.argv[1], proc.returncode))
        exit(proc.returncode)

if __name__ == '__main__':
    main()
