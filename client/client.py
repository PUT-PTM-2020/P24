import os
import socket
"""
REQUESTS
"""
START_SEND_FILE = '\x01'
NEXT_SEND_FILE = '\x02'
DOWNLOAD_FILE = '\x03'
GET_PATH_INFO = '\x04'
GET_FILE_INFO = '\x05'
CHANGE_DIRECTORY = '\x06'
DELETE_FILE = '\x07'
DELETE_DIRECTORY = '\x08'
GET_FILE = '\x09'
CREATE_DIRECTORY = '\x0a'
END_SEND_FILE = '\x0b'
LOGIN = '\x0c'
LOGOUT = '\x0d'
PUBLIC = '\x0e'

RESP_OK = '\x00'
RESP_ERR = '\x01'
RESP_MORE = '\x02'
RESP_AUTH_ERR = '\x03'
RESP_NO_PATH = '\x04'


class Client:

    def __init__(self, user=''):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind((socket.gethostbyname(socket.gethostname()),20000))
        self.SERVER_ADDRESS = ("192.168.1.78", 20000)
        self.HEADER_SIZE = 6
        self.USER = 'public'
        self.file_control = b'\x00\x00\x00'
        self.ID = b'\x00'
        self.user_dir = os.getcwd() #+ "\\" + user
        self.remote_dir_path = '/'
        self.home_dir = '/'+ self.USER
        self.public_dir = '/public'


    def show_local_files(self):
        files = os.listdir()
        for file,x in zip(files, range(1, len(files)+1)):
            print(file, end='   ')
            if x % 5 == 0:
                print('', end='\n')
        print('')

    def show_local_directory(self):
        print(os.getcwd())

    def send_and_recv_packet(self, operation, data='', answer='\x00',file_ctrl=b'\x00\x00\x00'):
        it = 0
        recv_packet = None
        try:
            self.socket.settimeout(0.1)
            packet, address = self.socket.recvfrom(1024)
        except socket.timeout:
            pass
        finally:
            packet = b'\x00'
        while recv_packet is None and it < 10:
            try:
                header = operation.encode('utf-8') + answer.encode('utf-8') + self.ID + file_ctrl + self.USER.encode('utf-8')
                if(type(data) == str):
                    data = data.encode('utf-8')
                packet = header + data
                print(packet)
                self.socket.sendto(packet, self.SERVER_ADDRESS)
                self.socket.settimeout(0.2)
                packet, address = self.socket.recvfrom(1024)
                if address != self.SERVER_ADDRESS:
                    continue
                if len(packet)>4:
                    print(packet[1])
                return packet
            except socket.timeout:
                it += 1
        print("Failed to connect")
        return bytes(''.encode("utf-8"))


    def get_remote_directory(self):
        print(self.remote_dir_path)


    def get_remote_fileinfo(self):
        try:
            packet = self.send_and_recv_packet(GET_FILE_INFO, data=self.remote_dir_path)
            if len(packet) == 0:
                return
            data = packet[self.HEADER_SIZE:].decode('utf-8')
            packet = packet[:3].decode('utf-8')
            if packet[0] == GET_FILE_INFO and packet[1] == RESP_OK:
                files = data.split(sep='*')
                for file in files:
                    print(file)
            if packet[1] != RESP_OK:
                print("Server returned error")
        except UnicodeDecodeError:
            print("Decode error")
            print(packet)


    def change_local_directory(self, path):
        try:
            os.chdir(path)
        except FileNotFoundError:
            print("Can't find directory")


    def change_remote_directory(self, path='/'):
        if path == '/' and self.USER == 'admin0':
            self.remote_dir_path = '/'
            return
        elif path == '/' and self.USER == 'public':
            self.remote_dir_path = self.public_dir_dir
            print("Access denied.")
            return
        elif path == '/':
            print("Access denied.")
            self.remote_dir_path = self.home_dir
            return

        if path == '..':
            if self.remote_dir_path != self.home_dir and self.remote_dir_path != self.public_dir:
                path = self.remote_dir_path.split(sep='/')
                self.remote_dir_path = '/'.join(path[:-1])
                if self.remote_dir_path == '':
                    self.remote_dir_path = '/'
                print(self.remote_dir_path)
                return
            else:
                print("Access denied!")
                return

        print("sysys")
        if path[0] != '/':
            if self.remote_dir_path == '/':
                path = self.remote_dir_path + path
            else:
                path = self.remote_dir_path + '/' + path
        packet = self.send_and_recv_packet(CHANGE_DIRECTORY, path)
        data = packet[5:].decode('utf-8')
        packet = packet[:3].decode('utf-8')
        if packet[0] == CHANGE_DIRECTORY and packet[1] == RESP_OK:
            self.remote_dir_path = path
            print(self.remote_dir_path)
        elif packet[1] == CHANGE_DIRECTORY and packet[1] == RESP_NO_PATH:
            print("Server: Invalid path")
        else:
            print("Network Error... Try again")

    def send_file(self, filename):
        try:
            op_type = 'rb'
            if filename[-4:] == '.txt':
                op_type = 'r'
            with open(filename, op_type) as file:
                if self.remote_dir_path == '/':
                    path = self.remote_dir_path + self.USER
                else:
                    path = self.remote_dir_path + '/' + filename
                freebuffer = 512 - self.HEADER_SIZE - len(path) - 1
                msg_buffer = file.read(freebuffer)
                if type(msg_buffer) == str:
                    msg_buffer = msg_buffer.encode('utf-8')
                total_packets_recieved = self.file_control
                total_packets_recieved = self.iterate_byte(total_packets_recieved)
                packet = self.send_and_recv_packet(START_SEND_FILE,file_ctrl=total_packets_recieved , data=(path + '*').encode('utf-8') + msg_buffer)
                packet = packet[:3].decode('utf-8')
                if packet[0] == START_SEND_FILE and packet[1] == RESP_OK:
                    total_packets_recieved = self.iterate_byte(total_packets_recieved)
                else:
                    print("Error during upload...")
                    return
                msg_buffer = file.read(freebuffer)

                while msg_buffer:
                    if type(msg_buffer) == str:
                        msg_buffer = msg_buffer.encode('utf-8')
                    packet = self.send_and_recv_packet(NEXT_SEND_FILE,file_ctrl=total_packets_recieved ,data=(path + '*').encode('utf-8')  + msg_buffer)
                    packet = packet[:3].decode('utf-8')
                    if packet[0] == NEXT_SEND_FILE and packet[1] == RESP_OK:
                        total_packets_recieved = self.iterate_byte(total_packets_recieved)
                    else:
                        print("Error during upload.")
                        break
                    msg_buffer = file.read(freebuffer)
            packet = self.send_and_recv_packet(END_SEND_FILE)
            packet = packet[:3].decode('utf-8')
            if packet[1] == RESP_OK:
                print("File sent")
        except FileNotFoundError:
            print("File does not exist.")


    def get_file(self, filename):
        local_files = os.listdir()
        if filename in local_files:
            print("File already exists")
            return
        op_type = 'a+b'
        if filename[-4:] == '.txt':
                op_type = 'a+'
                file = open(filename, op_type, newline='')
        else:
            file = open(filename, op_type)
        path = ''
        if self.remote_dir_path == '/':
            path = self.remote_dir_path+filename
        else:
            path = self.remote_dir_path + '/' + filename
        total_packets_recieved = self.file_control
        packet = self.send_and_recv_packet(GET_FILE, path, file_ctrl=total_packets_recieved)
        data = packet[5:]
        if op_type == 'a+':
            data = data.decode('utf-8')
        packet = packet[:3].decode('utf-8')
        if packet[1] == RESP_MORE:
            file.write(data)
            print(data, end='')
            total_packets_recieved = self.iterate_byte(total_packets_recieved)
            while(packet[1] == RESP_MORE):
                packet = self.send_and_recv_packet(GET_FILE, path, file_ctrl=total_packets_recieved)
                data = packet[5:]
                if op_type == 'a+':
                    data = data.decode('utf-8')
                packet = packet[:3].decode('utf-8')
                if packet[1] == RESP_MORE:
                    file.write(data)
                    total_packets_recieved = self.iterate_byte(total_packets_recieved)
                elif packet[1] == RESP_OK:
                    file.write(data)
                    print("\nFile received")
                    break
                elif packet[1] == RESP_ERR:
                    print("Error while downloading")
        elif packet[1] == RESP_OK:
            file.write(data)
            print(data)
            print("File received, no loop")
            file.close()
            return

        elif packet[1] == RESP_ERR:
            print("File does not exist")
            file.close()
            return



    def delete_file(self, filename):
        path =''
        if self.remote_dir_path == '/':
            path = self.remote_dir_path + filename
        else:
            path = self.remote_dir_path + '/' + filename
        packet = self.send_and_recv_packet(DELETE_FILE, path)
        data = packet[5:].decode('utf-8')
        packet = packet[:3].decode('utf-8')
        if packet[0] != DELETE_FILE or packet[1] == RESP_ERR:
            print("Network Error")
        else:
            print("File has been deleted.")

    def create_directory(self, dirname):
        path = ''
        if self.remote_dir_path == '/':
            path = self.remote_dir_path + dirname
        else:
            path = self.remote_dir_path + '/' + dirname
        packet = self.send_and_recv_packet(CREATE_DIRECTORY, path)
        data = packet[5:].decode('utf-8')
        packet = packet[:3].decode('utf-8')
        if packet[0] != CREATE_DIRECTORY or packet[1] == RESP_ERR:
            print("Network Error")
        elif packet[1] == RESP_OK:
            print("Directory has been created.")
        else:
            print("Error")

    def delete_directory(self, dirname):
        path = ''
        if self.remote_dir_path == '/':
            path = self.remote_dir_path + dirname
        else:
            path = self.remote_dir_path + '/' + dirname
        packet = self.send_and_recv_packet(DELETE_DIRECTORY, path)
        data = packet[5:].decode('utf-8')
        packet = packet[:3].decode('utf-8')
        if packet[0] != DELETE_DIRECTORY or packet[1] == RESP_ERR:
            print("Network Error")
        else:
            print("Directory has been deleted.")


    def iterate_byte(self, byte):
        val = int.from_bytes(byte, 'big')
        val += 1
        return val.to_bytes(3, 'big')

    def login(self, username):
        if len(username) != 6:
            print("Wrong length of username")
            return
        password = input("Password: ")
        if len(password) != 8:
            print("Wrong length of password")
            return
        self.USER = username
        packet = self.send_and_recv_packet(LOGIN, password)
        print(packet)
        result = self.check_data(LOGIN, packet)
        if result == 0:
            print("Login successful")
            print(packet[2])
            self.ID = packet[2].to_bytes(1,'big')
            self.home_dir = self.remote_dir_path = '/' + self.USER

            return 1
        else:
            self.USER = 'public'
            print("Err")
            return 0


    def check_data(self, op, packet):
        if len(packet)<1:
            return -1
        data = packet[5:].decode('utf-8')
        packet = packet[:3].decode('utf-8')
        if packet[0] != op or packet[1] == RESP_ERR:
            return -1
        else:
            return 0

    def public(self):
        packet = self.send_and_recv_packet(PUBLIC)
        result = self.check_data(PUBLIC, packet)
        if result == 0:
            print("Logged in as guest")
            self.ID = packet[2].to_bytes(1, 'big')
            self.home_dir = self.remote_dir_path = '/' + self.USER
            return 1
        return 0

    def exit(self):
        result = 1
        packet = self.send_and_recv_packet(LOGOUT)
        result = self.check_data(LOGIN, packet)

        return 1


if __name__ == '__main__':
    client = Client()
    isLogged = 0;
    while not isLogged:
        command = ''
        command = input("LOGIN or PUBLIC:").upper()
        if command == 'PUBLIC':
            isLogged = client.public()
        elif command == 'LOGIN':
            username = input("username: ")
            isLogged = client.login(username)
        else:
            print("Command unknown")

    exit = 0
    while not exit:
        command = ''
        command = input(">>").split()
        command[0] = command[0].upper()
        if command[0] == 'HELP':
            print("Commands:\n",
                  "File upload/download:             SEND   GET \n",
                  "List local/remote files:          LF     RF \n",
                  "Current local/remote directory:   CWD    CWRD \n",
                  "Change local/remote directory:    CD     CRD \n",
                  "Create/Delete remote directory:   ND     DD \n",
                  "Delete file/leave program:        DEL    EXIT \n",
                  )
        elif command[0] == 'CWD':
            client.show_local_directory()
        elif command[0] == 'LF':
            client.show_local_files()
        elif command[0] == 'RF':
            client.get_remote_fileinfo()
        elif command[0] == 'CWRD':
            client.get_remote_directory()
        elif command[0] == 'CD':
            try:
                client.change_local_directory(command[1])
            except IndexError:
                print("Specify new path")
        elif command[0] == 'CRD':
            try:
                client.change_remote_directory(command[1])
            except IndexError:
                client.change_remote_directory('/')
        elif command[0] == 'SEND':
            try:
                client.send_file(command[1])
            except IndexError:
                print("Give filename")
        elif command[0] == 'GET':
            try:
                print(command[1:])
                client.get_file(" ".join(command[1:]))
            except IndexError:
                print("Give filename")
        elif command[0] == 'DEL':
            try:
                client.delete_file(command[1])
            except IndexError:
                print("Give filename")
        elif command[0] == 'ND':
            try:
                client.create_directory(command[1])
            except IndexError:
                print("Give filename")
        elif command[0] == 'DD':
            try:
                client.delete_directory(command[1])
            except IndexError:
                print("Give filename")
        elif command[0] == "EXIT":
            end = client.exit()
            break
