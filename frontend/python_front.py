from uuid import uuid4
from datetime import datetime
from nicegui import ui, app

# this is just storing all the messages for now
# i need help replacing this with the real backend
messages = []  # each message is a dict with user + text + time

# just gets the current time for the message
def timestamp() -> str:
    return datetime.now().strftime('%I:%M %p').lstrip('0')


@ui.refreshable
def chat_messages(own_id: str):
    # loop through recent messages and display them
    for m in messages[-200:]:
        ui.chat_message(
            avatar=m['avatar'],
            text=m['text'],
            sent=(m['user_id'] == own_id),  # checks if its your message
            name=('You' if m['user_id'] == own_id else m['name']),
            stamp=m['time'],  # shows the time
        )


@ui.page('/')
def index():
    # keeps track of each user separately per browser
    session = app.storage.user

    # give the user an id if they dont have one yet
    if 'user_id' not in session:
        session['user_id'] = str(uuid4())[:8]

    # default username
    if 'name' not in session:
        session['name'] = f'User-{session["user_id"]}'

    user_id = session['user_id']
    avatar = f'https://robohash.org/{user_id}?bgset=bg2'  # auto avatar

    # small styling so it looks nicer
    ui.add_css('''
    .chat-wrap { max-width: 900px; margin: 0 auto; }
    .nicegui-content { background: #f6f7fb; }
    ''')

    with ui.column().classes('chat-wrap w-full'):
        # top bar
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
                chat_messages(user_id)

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
                messages.append({
                    'user_id': user_id,
                    'name': session['name'],
                    'avatar': avatar,
                    'text': msg,
                    'time': timestamp(),
                })

                text.value = ''
                chat_messages.refresh()

            ui.button('Send', on_click=send).props('unelevated color=primary')
            text.on('keydown.enter', lambda _: send())

        # refreshes so multiple tabs update
        ui.timer(0.5, chat_messages.refresh)


# runs the nicegui app
ui.run(
    title='Chat Frontend',
    reload=False,
    storage_secret='dev-secret-key'
)
