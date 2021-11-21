use std::env;
use std::io::{stdin, stdout, Write};
use std::path::Path;
use std::process::{Child, Command, Stdio};
use termios::*;

fn clear_screen_and_start_top() -> std::io::Result<()> {
    stdout().write_all(b"\x1b[2J\x1b[H")?;
    Ok(())
}
fn set_raw_mode() -> std::io::Result<()> {
    // Tilda ~ is a bitwise NOT operator: 011100 -> 100011
    let mut raw = Termios::from_fd(0)?;
    // turns of ctr-S and ctrl-Q
    raw.c_iflag &= !(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // turns of ctr-S and ctrl-Q
    raw.c_oflag &= !(OPOST);
    // Turns off echo and Canonical mode and ctrl-c and ctrl-z
    raw.c_lflag &= !(ECHO | ICANON | IEXTEN | ISIG);
    // not needed for modern terminals. Sets character size to 8 bits per byte
    raw.c_cflag |= CS8;

    // minimum number of bytes for read to terminate
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    // TCSAFLUSH -> waits for all pending output to be written and discards any input
    // that hasn't been read.
    cfsetspeed(&mut raw, B9600)?;
    tcsetattr(0, TCSANOW, &raw)?;
    Ok(())
}
fn revert_raw_mode() -> std::io::Result<()> {
    let mut raw = Termios::from_fd(0).unwrap();
    tcsetattr(0, TCSANOW, &mut raw)?;
    Ok(())
}
fn main() {
    loop {
        // use the `>` character as the prompt
        // need to explicitly flush this to ensure it prints before read_line
        match clear_screen_and_start_top() {
            Ok(_) => (),
            Err(_) => {
                return;
            }
        }
        match set_raw_mode() {
            Ok(_) => {
                return;
            }
            Err(_) => {
                match revert_raw_mode() {
                    Ok(_) => {
                        println!("OK");
                    }
                    Err(_) => {
                        println!("oops");
                    }
                }
                return;
            }
        }
        //        match set_raw_mode() {
        //            Ok(_) => (),
        //            Err(_) => {
        //                return;
        //            }
        //        }
        print!("> ");
        stdout().flush().unwrap();

        let mut input = String::new();
        stdin().read_line(&mut input).unwrap();

        // read_line leaves a trailing newline, which trim removes
        // this needs to be peekable so we can determine when we are on the last command
        let mut commands = input.trim().split(" | ").peekable();
        let mut previous_command = None;

        while let Some(command) = commands.next() {
            // everything after the first whitespace character is interpreted as args to the command
            let mut parts = command.trim().split_whitespace();
            let command = parts.next().unwrap();
            let args = parts;
            println!("{}", &command);
            match command {
                "cd" => {
                    // default to '/' as new directory if one was not provided
                    let new_dir = args.peekable().peek().map_or("/", |x| *x);
                    let root = Path::new(new_dir);
                    if let Err(e) = env::set_current_dir(&root) {
                        eprintln!("{}", e);
                    }

                    previous_command = None;
                }
                "exit" => return,
                command => {
                    let stdin = previous_command.map_or(Stdio::inherit(), |output: Child| {
                        Stdio::from(output.stdout.unwrap())
                    });

                    let stdout = if commands.peek().is_some() {
                        // there is another command piped behind this one
                        // prepare to send output to the next command
                        Stdio::piped()
                    } else {
                        // there are no more commands piped behind this one
                        // send output to shell stdout
                        Stdio::inherit()
                    };

                    let output = Command::new(command)
                        .args(args)
                        .stdin(stdin)
                        .stdout(stdout)
                        .spawn();

                    match output {
                        Ok(output) => {
                            previous_command = Some(output);
                        }
                        Err(e) => {
                            previous_command = None;
                            eprintln!("{}", e);
                        }
                    };
                }
            }
        }

        if let Some(mut final_command) = previous_command {
            // block until the final command has finished
            final_command.wait().unwrap();
        }
    }
}
