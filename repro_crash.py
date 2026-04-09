import subprocess
import time

def test_engine():
    engine_path = "build_portal/pbrain-MINT-P"
    print(f"Testing engine at {engine_path}...")
    
    proc = subprocess.Popen(
        [engine_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )

    commands = [
        "YXSHOWINFO\n",
        "START 15\n",
        "INFO CLEARPORTALS\n",
        "INFO YXPORTAL 7,5 7,10\n",  # Vertical portal pair near center
        "YXBOARD\n",
        "7,7,1\n", # Place stone at (7,7) - near the portal path
        "8,8,2\n",
        "DONE\n",
        "YXNBEST 1\n"  # Start analysis - this is where it crashed
    ]

    try:
        for cmd in commands:
            print(f"Send: {cmd.strip()}")
            proc.stdin.write(cmd)
            proc.stdin.flush()
            # Brief sleep to let it process
            time.sleep(0.1)

        print("Waiting for search output (Depth 9 test)...")
        start_time = time.time()
        crashed = False
        while time.time() - start_time < 10:  # Wait up to 10s for some depth info
            line = proc.stdout.readline()
            if not line:
                if proc.poll() is not None:
                    print(f"CRASH DETECTED! Exit code: {proc.returncode}")
                    crashed = True
                    break
                continue
            
            print(f"Engine: {line.strip()}")
            if "depth 9" in line:
                print("SUCCESS: Search reached depth 9 without crashing!")
                break
        
        if not crashed:
            proc.stdin.write("STOP\n")
            proc.stdin.write("END\n")
            proc.stdin.flush()
            print("Cleanup sent.")

    except Exception as e:
        print(f"Error during test: {e}")
    finally:
        if proc.poll() is None:
            proc.terminate()
            
if __name__ == "__main__":
    test_engine()
