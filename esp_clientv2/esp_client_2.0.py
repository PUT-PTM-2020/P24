import socket
import os
from pathlib import Path
import select
from bitstring import BitArray

OP_INIT = '00000'
OP_LOGIN = '00001'
OP_SEND_FILE = '00010'
OP_GET_FILE = '00011'
OP_DEL_FILE = '00100'
OP_EXIT = '00101'
OP_SHOW_FILES = '00110'
RESPONSE = '0000'
MESSAGE_SIZE = '0000000000'
VERSION = '100'
OP_REGISTER = '00111'

NOT_ALLOWED = ['NEWACC']


class Client:

    def __init__(self, user=''):
        self.s = ''
        self.SERVER_ADDRESS = ("192.168.18.34", 80)
        self.HEADER_SIZE = 4
        self.USER = user
        self.ID = '00000000'
        self.user_dir = os.getcwd() + "\\" + user

    """
    Conversion/get functions
    """

    def bitstring_to_bytes(self, s):
        """""
        Converts string of bits into byte and returns that byte
        """""
        return int(s, 2).to_bytes(len(s) // 8, byteorder='big')

    def format_header(self, id, operation, response):
        """""
        connects bytes of header into single bytearray and returns it
        """""
        next_msg_size = '000000000'
        header = self.bitstring_to_bytes(VERSION + operation)
        header += self.bitstring_to_bytes(id)
        header += self.bitstring_to_bytes(response + '0000')
        header += self.bitstring_to_bytes('00000011')
        return header

    def msg_size_tobitstring(self, size):
        """""
                Converts string of bits into byte and returns that byte
                """""
        size = str(format(size, 'b'))
        for x in range(10 - len(size)):
            size = '0' + size
        return size

    def conv_header_to_bitstring(self, message):
        header = ''
        for byte in message:
            temp = bin(byte)[2:]
            for x in range(8 - len(temp)):
                temp = '0' + temp
            header += temp
        return header

    def get_next_msg_size(self, message):
        return self.msg_size_tobitstring(len(message))


    def recv_response_header(self):
        try:
            self.s.settimeout(20.0)
            print("waiting")
            header = self.s.recv(1024)
            return self.conv_header_to_bitstring(header)
        except socket.timeout:
            print("No response")
            self.s.shutdown(1)
            self.s.close()
            return -1


    """
    Initialization function
    """

    def init_connection(self):
        print("Connecting...")
        tries = 0
        while True:
            try:
                self.s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.s.connect(self.SERVER_ADDRESS)
                return 0
            except TimeoutError:
                print("Failed to connect...")
                tries += 1
                if tries < 3:
                    print("retrying")
                else:
                    return -1

    def send_message(self, header: bytes, message: bytes):
        it = 500 - len(header+message)
        for x in range(it):
            message = message + b'\x01'

        self.s.sendall(header + message)

    def interpret_header(self, header, expected_op):
        version = header[:3]
        operation = header[3:8]
        answer = header[16:20]
        id = header[8:16]
        next_msg = header[20:30]
        fulfillment = header[30:31]
        if version != '100':
            return -1
        if operation != expected_op:
            return -1
        if next_msg != '0000000000':
            return -1
        if fulfillment != '11':
            return -1
        if answer == '1111':
            if expected_op == OP_LOGIN:
                self.ID = id
            else:
                if self.ID != id:
                    return -1
        return 0

    def send_file_to_server(self, filename):
        self.init_connection()

    """
    Command Functions
    """

    def register(self):

        while True:
            username = input("Username(6 characters/numbers):")
            password = input("Password(max 6 characters/numbers):")
            password_check = input("Repeat password:")
            if password != password_check:
                print("Passwords do not match!")
                continue
            elif len(username) != 6:
                print("Wrong username length!")
                continue
            elif username in NOT_ALLOWED:
                print("Username not allowed!")
            elif not username.isalnum():
                print("Username isn't alphanumeric")
            elif not password.isalnum():
                print("Password isn't alphanumeric")
            else:
                break

        message = ('USER=' + self.USER + "PASS=" + password).encode()
        if self.init_connection() == 0:
            header = self.format_header(self.ID, OP_REGISTER, RESPONSE)
            self.send_message(header, message)
            response = self.recv_response_header()
            if response == -1:
                return -1
            response = self.interpret_header(response, OP_LOGIN)
            if response == -1:
                return -1
            print("User registered")
            return 0

    def login_to_server(self, pwd):
        message = 'USER=' + self.USER + "PASS=" + pwd
        message = message.encode('utf-8')
        if self.init_connection() == 0:
            header = self.format_header(self.ID, OP_LOGIN, RESPONSE)
            print(message)
            self.send_message(header, message)
            response = self.recv_response_header()
            if response == -1:
                return -1
            response = self.interpret_header(response, OP_LOGIN)
            if response == -1:
                return -1
        else:
            return -1
        user_path = Path(self.user_dir)
        if not user_path.is_dir():
            user_path.mkdir(parents=True, exist_ok=True)
        os.chdir(self.user_dir)
        return 0

    def show_local_files(self):
        files = os.listdir()
        for file, x in files, range(len(files)):
            print(file, end='   ')
            if x % 5 == 0:
                print('', end='\n')


if __name__ == '__main__':
    client = Client()
    while True:
        print("Write NEWACC if not registered yet.")
        usr = input("LOGIN (6 characters):\n")
        if len(usr) != 6:
            print("Wrong size")
            continue
        client.USER = usr
        if usr == 'NEWACC':
            client.register()
        else:
            pwd = input("PASSWORD:\n")
            client = Client(usr)
            if client.login_to_server(pwd) == 0:
                break
            else:
                print("Error! Try to connect later")
    while True:
        command = input(">>").split()
        command[0].upper()
        if command[0] == 'HELP':
            print("Commands:\n   SEND   GET   SHOW\n",
                  "LOCAL   CD   DEL   EXIT\n")
        elif command[0] == 'SEND':
            client.send_file_to_server(command[1])
        elif command[0] == 'LOCAL':
            client.show_local_files()
        elif command[0] == 'SHOW':
            client.show_local_files()
        elif command[0] == 'EXIT':
            break
