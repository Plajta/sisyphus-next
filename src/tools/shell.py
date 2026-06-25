#!/usr/bin/env python3
import cmd
import readline
import glob
import os
import sys
import protocol
from rich.console import Console
from rich.progress import Progress

console = Console()

class DeviceShell(cmd.Cmd):
    prompt = "\033[1;36mSisyphus>\033[0m "

    def __init__(self, device):
        super().__init__()
        self.device = device
        # cache remote files for completion
        self.autofill_remote_refresh()

    def preloop(self):
        readline.parse_and_bind('tab: complete')

    def autofill_remote_refresh(self):
        files = self.device.ls()
        remote_files = []
        for file in files:
            remote_files.append(file.name)
        self.remote_files = remote_files

    def do_ls(self, arg):
        "ls                List remote files"
        files = self.device.ls()
        console.print("[red]<Filename>[/red] [green]<Size in bytes>[/green]")

        remote_files = [] # Update it anyway, just in case
        for file in files:
            remote_files.append(file.name) # Update it anyway, just in case

            console.print(f"[red]{file.name}[/red] [green]{file.size}[/green]")
        self.remote_files = remote_files

    def complete_ls(self, text, line, begidx, endidx):
        return []  # no args

    def do_rm(self, arg):
        "rm <filename>     Remove remote file"
        if not arg:
            console.print("[bold red]Usage: rm <filename>[/bold red]")
        elif arg not in self.remote_files:
            console.print(f"[bold red]No such file: {arg}[/bold red]")
        else:
            status, resp = self.device.rm(arg)
            if status:
                console.print(f"[green]File [bold]{arg}[/bold] removed")
            else:
                console.print(f"[bold red]Error: {resp}[/bold red]")
                self.autofill_remote_refresh()

    def complete_rm(self, text, line, begidx, endidx):
        return [f for f in self.remote_files if f.startswith(text)]

    def do_mv(self, arg):
        "mv <old> <new>    Rename remote file"

        parts = arg.split()
        if len(parts) != 2:
            console.print("[bold red]Usage: mv <old> <new>[/bold red]")
            return
        old, new = parts

        if old not in self.remote_files:
            console.print(f"[bold red]No such file: {old}[/bold red]")
            return

        status, resp = self.device.mv(old, new)
        if status:
            console.print(f"[green]File [bold]{old}[/bold] moved to [bold]{new}[/bold]")
        else:
            console.print(f"[bold red]Error: {resp}[/bold red]")

        self.autofill_remote_refresh()

    def complete_mv(self, text, line, begidx, endidx):
        args = line.split()
        if len(args) == 2:
            return [f for f in self.remote_files if f.startswith(text)]
        return []

    def do_push(self, arg):
        "push <local> [remote]   Upload file to device"

        parts = arg.split()
        if not parts:
            console.print("[bold red]Usage: push <local> [remote][/bold red]")
            return
        local = parts[0]
        remote = parts[1] if len(parts)==2 else os.path.basename(local)

        if not os.path.isfile(local):
            console.print(f"[bold red]Local file not found: {local}[/bold red]")
            return
        size = os.path.getsize(local)

        console.print(f"[yellow]Uploading {local} as {remote} ({size} bytes)...[/yellow]")

        # with open(local,'rb') as f:
        #     checksum = crc32(f.read())

        # self.device.send_command(f"push {remote} {size} {checksum}")

        progress = Progress()
        task = progress.add_task("[cyan]Transferring file...", total=size)

        def update_progress(transferred, speed):
            speed_str = f"Speed: {speed:.2f} KB/s"
            progress.update(task, completed=transferred, description=f"[cyan]{speed_str}")

        with progress:
            with open(local,'rb') as f:
                status, resp = self.device.push(f, size, remote, update_progress)

        if status:
            console.print(f"[green]File [bold]{remote}[/bold] uploaded to device")
        else:
            console.print(f"[bold red]Error: {resp}[/bold red]")
        self.autofill_remote_refresh()

    def complete_push(self, text, line, begidx, endidx):
        parts = line.split()

        if len(parts) <= 2:
            return glob.glob(text+'*')
        elif len(parts) == 3:
            return [f for f in self.remote_files if f.startswith(text)]
        return []

    def do_pull(self, arg):
        "pull <remote> [local]  Download file from device"

        parts = arg.split()
        if not parts:
            console.print("[bold red]Usage: pull <remote> [local][/bold red]")
            return
        remote = parts[0]
        local = parts[1] if len(parts)==2 else remote

        console.print(f"[yellow]Requesting {remote}...[/yellow]")

        progress = Progress()
        task = None

        def create_task(total):
            nonlocal task
            task = progress.add_task("[cyan]Transferring file...", total=total)

        def update_progress(transferred, speed):
            speed_str = f"Speed: {speed:.2f} KB/s"
            progress.update(task, completed=transferred, description=f"[cyan]{speed_str}")

        with progress:
            with open(local,'wb') as f:
                status, resp = self.device.pull(remote, f, update_progress, create_task)

        if status:
            console.print(f"[bold green]File {local} downloaded[/bold green]")
        else:
            console.print(f"[bold red]Error: {resp}[/bold red]")

    def complete_pull(self, text, line, begidx, endidx):
        parts = line.split()
        if len(parts) <= 2:
            return [f for f in self.remote_files if f.startswith(text)]
        elif len(parts) == 3:
            return glob.glob(text+'*')
        return []

    def do_play(self, arg):
        "play <filename>     Play a remote file"
        if not arg:
            console.print("[bold red]Usage: play <filename>[/bold red]")
        elif arg not in self.remote_files:
            console.print(f"[bold red]No such file: {arg}[/bold red]")
        else:
            status, resp = self.device.play(arg)
            if status:
                console.print(f"[green]File [bold]{arg}[/bold] played")
            else:
                console.print(f"[bold red]Error: {resp}[/bold red]")

    def complete_play(self, text, line, begidx, endidx):
        return [f for f in self.remote_files if f.startswith(text)]

    def do_info(self, arg):
        "info              Get information about the device"
        info = self.device.info()
        console.print(f"[yellow]Device Name: [/yellow][red]{info.device_name}[/red]")
        console.print(f"[yellow]Protocol version: [/yellow][red]{info.protocol_version}[/red]")
        console.print(f"[yellow]Git SHA: [/yellow][red]{info.git_commit_sha}[/red]")
        console.print(f"[yellow]Build date: [/yellow][red]{info.build_date}[/red]")
        console.print(f"[yellow]Filesystem: [/yellow][red]{info.free_space/1024} / {info.fs_size/1024} KB used[/red]")
        console.print(f"[yellow]Uses Eternity: [/yellow][red]{info.uses_eternity}[/red]")
        console.print(f"[yellow]Debug mode: [/yellow][red]{info.debug_mode}[/red]")
        if info.debug_mode:
            console.print("[bold red]DO NOT SHIP THIS FIRMWARE, OR DEVICE!!!\nDEBUG MODE IS ON[/bold red]")

    def complete_info(self, text, line, begidx, endidx):
        return []  # no args

    def do_measure(self, arg):
        "measure              Get a color measurement from the device's color sensor"
        color = self.device.measure()
        console.print(f"[magenta]Hue: {color.hue}[/magenta]")
        console.print(f"[yellow]Saturation: {color.saturation}[/yellow]")
        console.print(f"[cyan]Value: {color.value}[/cyan]")
        console.print(f"[white]Clear: {color.clear}[/white]")

    def complete_measure(self, text, line, begidx, endidx):
        return []  # no args

    def do_reset(self, arg):
        "reset              Reset the device to bootloader"
        self.device.reset()
        return self.do_exit('')

    def complete_reset(self, text, line, begidx, endidx):
        parts = line.split()
        if len(parts) <= 2:
            return ['boot']
        return []

    def do_exit(self, arg):
        "exit                Exit shell"
        console.print("[bold cyan]Goodbye![/bold cyan]")
        return True
    do_quit = do_exit

if __name__ == '__main__':
    device = protocol.ProtocolClient()
    shell = DeviceShell(device)
    try:
        shell.cmdloop()
    except KeyboardInterrupt:
        print("") # Just so it's on a new line :)
        shell.do_exit('')
        sys.exit(0)
