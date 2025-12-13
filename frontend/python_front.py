from uuid import uuid4
from datetime import datetime
from nicegui import ui, app
import os
from collections import deque
import subprocess
# this is just storing all the messages for now
# i need help replacing this with the real backend
#messages = []  # each message is a dict with user + text + time

# just gets the current time for the message
def timestamp() -> str:
    return datetime.now().strftime('%I:%M %p').lstrip('0')


@ui.refreshable
def chat_messages(own_id: str,messages,pipe_path):
    # loop through recent messages and display them
    fd = os.open(pipe_path, os.O_RDONLY | os.O_NONBLOCK)
    #This use of an fd is to use a nonblock flag
    with os.fdopen(fd) as backendpipe:
        try:
            while True:
                #This is sorta hacky but this prevents a block
                line = backendpipe.readline()
                if not line:  # no data right now
                    break
                line = line.strip()
                if line.startswith("[read]"):
                    line = line.replace("[read]", "", 1)
                    messages.append(line)
        except BlockingIOError:
            pass
    
    #given it's now a queue with maxlenght of 200
    for m in messages:
        # this logic will have to be entirely reworked
        msgid = m.split(":",1)[0]
        msg = m.split(":",1)[1]
        
        #reduced to bare essentials for our own sake
        ui.chat_message(
            text=msg,
            sent=(msgid == own_id),  # checks if its your message
            name=('You' if msgid == own_id else msgid),
        )
        


@ui.page('/')
def index():
    # moving it into index so it's locally scoped per client
    messages = deque(maxlen=200)
    # keeps track of each user separately per browser
    session = app.storage.user

    # give the user an id if they dont have one yet
    if 'user_id' not in session:
        session['user_id'] = str(uuid4())[:8]

    # default username
    if 'name' not in session:
        session['name'] = f'User-{session["user_id"]}'

    user_id = session['user_id'] 
    print(f"Client {user_id} connected.")

    #initialize the named pipe
    pipe_path = "/tmp/"+str(user_id)
    try:
        os.mkfifo(pipe_path)
    except FileExistsError:
        print(f"Error intilizing pipe {pipe_path}, does it allready exist?")
    
    Process = subprocess.Popen(["../client/Client",pipe_path],stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


    # small styling so it looks nicer
    ui.add_css('''
    .chat-wrap { max-width: 900px; margin: 0 auto; }
    .nicegui-content { background: #f6f7fb; }
    ''')

    with ui.column().classes('chat-wrap w-full'):
        # top bar
        #right now this is useless, I cna probbaly implement this super easily*
        with ui.row().classes('w-full items-center justify-between q-pa-sm'):
            ui.label('CMPSC 311Chat Room ').classes('text-2xl font-bold')

            # lets user change their display name
            with ui.row().classes('items-center'):
                name_input = ui.input(
                    'enter display name:',
                    value=session['name']
                ).props('dense outlined')

                ui.button(
                    'Update',
                    on_click=lambda: session.__setitem__('name', name_input.value)
                ).props('outline')

        ui.separator()

        # main chat area
        with ui.card().classes('w-full').style('height: 65vh; overflow-y: auto;'):
            with ui.column().classes('w-full'):
                chat_messages(user_id,messages,pipe_path)

        # message input area
        with ui.row().classes('w-full items-center q-pa-sm'):
            text = ui.input(
                placeholder='Type a message...'
            ).props('outlined').classes('flex-grow')

            # send button logic
            def send():
                msg = (text.value or '').strip()
                if not msg:
                    return

                # add message to list (ui-only for now)
                """
                messages.append({
                    'user_id': user_id,
                    'name': session['name'],
                    'text': msg,
                    'time': timestamp(),
                })
                """

                text.value = ''
                #this will block with nobody to read it but-
                #Honestly, if somehow the backend crashes something is wrong
                with open(pipe_path,"w") as backendpipe:
                    backendpipe.write(f"[Sent]{user_id}:{msg}\n")
                    backendpipe.flush()
                    

            ui.button('Send', on_click=send).props('unelevated color=primary')
            text.on('keydown.enter', lambda _: send())

        # refreshes so multiple tabs update
        ui.timer(0.5, lambda: chat_messages.refresh(user_id,messages,pipe_path))


# runs the nicegui app
ui.run(
    title='Chat Frontend',
    reload=False,
    storage_secret='dev-secret-key'
)
