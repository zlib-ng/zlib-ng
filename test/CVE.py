import sys
import subprocess

def main():
    cmd = sys.argv[2:]
    proc = subprocess.Popen(cmd,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT)
    stdout, stderr = proc.communicate()
    print stdout
    #print stderr
    # We expect 1 error code is OK and we expect 134 or 
    # similar exit code for a vulnerable failure
    if proc.returncode == 1 or proc.returncode == 0:
        print('zlib not vulnerable to {0} ({1})'.format(sys.argv[1], proc.returncode))
        exit(0)
    else:
        print('zlib VULNERABLE to {0} ({1})'.format(sys.argv[1], proc.returncode))
        exit(proc.returncode)

if __name__ == '__main__':
    main()
