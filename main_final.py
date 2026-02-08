from flask import Flask,jsonify, request, make_response, redirect, url_for, render_template, session 
import mysql.connector

app =Flask(__name__)
app.secret_key = 'neka_super_tajna_sifra'

db_config = {
    'host': '127.0.0.1',
    'user': 'root',
    'password': 'Test1234!',
    'database': 'purs'
}

#AUTHORIZED_CARDS = ["[53, 14, AB, A5]"]

def get_db_connection():
    return mysql.connector.connect(**db_config)

@app.route("/")
def index():
    con = get_db_connection()
    cursor = con.cursor(dictionary=True)

    podaci = []
    ovlastene = []
    ulasci = []
    try:
        cursor.execute("SELECT * FROM sensor ORDER BY id DESC LIMIT 1")

        podaci = cursor.fetchall()
        for r in podaci:
         if r.get("lux") is None:
            r["lux"] = 0
         if r.get("gas") is None:
            r["gas"] = 0
         if r.get("humidity") is None:
            r["humidity"] = 0
         if r.get("temperature") is None:
            r["temperature"] = 0
         if r.get("ventilator") is None:
            r["ventilator"] = 0
         if r.get("ir_state") is None:
            r["ir_state"] = 0
        print("SENZORI:", podaci[:1])
    except Exception as e:
        print("SENSOR ERROR:", e)

    # --- RFID USERS ---
    try:
        cursor.execute("""
            SELECT owner_name as vlasnik, uid 
            FROM authorized_cards 
            ORDER BY id DESC
        """)
        ovlastene = cursor.fetchall()
    except Exception as e:
        print("RFID USERS ERROR:", e)

    # --- RFID LOGS ---
    try:
        cursor.execute("""
            SELECT uid, created_at AS vrijeme_ulaska
            FROM entry_logs
            ORDER BY id DESC
            LIMIT 10
        """)
        ulasci = cursor.fetchall()
    except Exception as e:
        print("RFID LOG ERROR:", e)

    cursor.close()
    con.close()

    return render_template(
        "index.html",
        podaci=podaci,
        ovlastene=ovlastene,
        ulasci=ulasci,
        user="Admin"
    )

@app.route('/check_card', methods=['POST'])
def check_card():
    data = request.get_json()
    card_uid = data.get("uid")
    
    con = get_db_connection()
    cursor = con.cursor()
    
    # Provjeravamo postoji li taj UID u bazi
    cursor.execute("SELECT * FROM authorized_cards WHERE uid = %s", (card_uid,))
    result = cursor.fetchone()
    
    if result:
        # Ako postoji, zabilježi ulazak u entry_logs
        cursor.execute("INSERT INTO entry_logs (uid) VALUES (%s)", (card_uid,))
        con.commit()
        cursor.close()
        con.close()
        return jsonify({"Pristup": "prihvačen"}), 200
    else:
        cursor.close()
        con.close()
        return jsonify({"Pristup": "odbijen"}), 403

@app.route('/api/sensor', methods=['POST'])
def primi_podatke():
    data = request.get_json(silent=True) or {}
    print("RAW JSON:", data)
    temp = data.get('temperature')
    vlaga = data.get('humidity')
    status_ventilatora = data.get('ventilator')
    plin_detektiran = data.get('gas')
    razina_svjetla = data.get('lux')
    ir_state = data.get('ir_state')
    hall_state = data.get('hall_state')
    parking_zauzeto = data.get('parking')
    con = get_db_connection()
    cursor = con.cursor()
    try:
        inserted_sensor = 0
        inserted_parking = 0
        if temp is not None and vlaga is not None:
            sql_temp = """
                INSERT INTO sensor
                  (temperature, humidity, gas, lux, ir_state, hall_state, ventilator)
                VALUES (%s, %s, %s, %s, %s, %s, %s)
            """
            cursor.execute(sql_temp, (
                temp,
                vlaga,
                plin_detektiran,
                razina_svjetla,
                ir_state,
                hall_state,
                status_ventilatora
            ))
            inserted_sensor = cursor.rowcount
            print("Inserted sensor id:", cursor.lastrowid)

        # UPIŠI parking status u parking_log
        if parking_zauzeto is not None:
            sql_parking = "INSERT INTO parking_log (zauzeto) VALUES (%s)"
            cursor.execute(sql_parking, (int(parking_zauzeto),))
            inserted_parking = cursor.rowcount
            print("Inserted parking id:", cursor.lastrowid)

        con.commit()

        return jsonify({
            'status': 'uspjeh',
            'inserted_sensor': inserted_sensor,
            'inserted_parking': inserted_parking
        }), 201

    except Exception as e:
        con.rollback()
        print("SQL ERROR:", e)
        return jsonify({'error': str(e)}), 500
    finally:
        cursor.close()
        con.close()

@app.route('/api/parking-status')
def parking_status():
    con = get_db_connection()
    cursor = con.cursor(dictionary=True)
    # Dohvaćamo samo zadnji status iz tablice parking_log za "živo" svjetlo na webu
    cursor.execute("SELECT zauzeto FROM parking_log ORDER BY id DESC LIMIT 1")
    rezultat = cursor.fetchone()
    cursor.close()
    con.close()
    
    status = rezultat['zauzeto'] if rezultat else 0
    return jsonify({'zauzeto': bool(status)})



@app.route('/login', methods=['GET'])
def login_get():
  return render_template('login.html')


@app.route('/login', methods=['POST'])
def login_post():
    #data = request.get_json()
    username = request.form.get('username')
    password = request.form.get ('password')

    if username == 'PURS' and password =='1234':
       session['korisnik'] = username
       return redirect(url_for('index'))
    else:
       return render_template('login.html', error="Pogrešno korisničko ime ili lozinka")

@app.route('/logout')
def logout():
    
    session.pop('korisnik', None)
    return redirect(url_for('login_get'))

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=False)
