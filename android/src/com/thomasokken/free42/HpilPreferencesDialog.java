/*****************************************************************************
 * FreeIL -- an HP-IL loop emulation
 * Copyright (C) 2014-2016 Jean-Christophe HESSEMANN
 *
 * This program is free software// you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY// without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program// if not, see http://www.gnu.org/licenses/.
 *****************************************************************************/

package com.thomasokken.free42;

import java.util.Set;

import android.app.Dialog;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.Spinner;



public class HpilPreferencesDialog extends Dialog {

	private OkListener okListener;
	private RadioButton hpilOffRB;
	private RadioButton hpilModeIPRB;
	private RadioButton hpilModePilBoxRB;
	private RadioButton hpilModeBluetoothRB;
	private Spinner hpilBTPairingSP;
	private Spinner hpilLatencySP;
	private EditText hpilAddrET;
	private EditText hpilOutPortET;
	private EditText hpilInPortET;
	private CheckBox hpilDebugCB;


	public HpilPreferencesDialog(Context context) {
        super(context);
        setContentView(R.layout.hpil_preferences_dialog);
        
        hpilOffRB = (RadioButton) findViewById(R.id.hpilOffRB); 
        hpilOffRB.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				setModeAvailables();
			}
		});
        hpilModeIPRB = (RadioButton) findViewById(R.id.hpilModeIPRB); 
        hpilModeIPRB.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				setModeAvailables();
			}
		});
        hpilModePilBoxRB = (RadioButton) findViewById(R.id.hpilModePilBoxRB); 
        hpilModePilBoxRB.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				setModeAvailables();
			}
		});
        hpilModeBluetoothRB = (RadioButton) findViewById(R.id.hpilModeBluetoothRB);
        hpilModeBluetoothRB.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				setModeAvailables();
			}
		});
        
        hpilBTPairingSP = (Spinner) findViewById(R.id.hpilBTPairingSP);
		Set<BluetoothDevice> setPairedDevices = BluetoothAdapter.getDefaultAdapter().getBondedDevices();
		BluetoothDevice[] pairedDevices = (BluetoothDevice[]) setPairedDevices.toArray(new BluetoothDevice[setPairedDevices.size()]);
        int i;
        String[] values = new String[pairedDevices.length];
		for (i = 0; i < pairedDevices.length; i++) {
			values[i] = pairedDevices[i].getName();
		}
		ArrayAdapter<String> aa = new ArrayAdapter<String>(context, android.R.layout.simple_spinner_item, values);
        hpilBTPairingSP.setAdapter(aa);
        
        hpilAddrET = (EditText) findViewById(R.id.hpilAddrET);
        hpilOutPortET = (EditText) findViewById(R.id.hpilOutPortET);
        hpilInPortET = (EditText) findViewById(R.id.hpilInPortET);
        
        hpilLatencySP = (Spinner) findViewById(R.id.hpilLatencySP);
        values = new String[] { "1", "2", "4", "8" };
        aa = new ArrayAdapter<String>(context, android.R.layout.simple_spinner_item, values);
        hpilLatencySP.setAdapter(aa);
        
        hpilDebugCB = (CheckBox) findViewById(R.id.hpilDebugCB);
        
        Button hpilOkB = (Button) findViewById(R.id.hpilOkB);
        hpilOkB.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                if (okListener != null)
                    okListener.okPressed();
                HpilPreferencesDialog.this.hide();
            }
        });
        
        Button hpilCancelB = (Button) findViewById(R.id.hpilCancelB);
        hpilCancelB.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                HpilPreferencesDialog.this.hide();
            }
        });
        
        setTitle("HP-IL Preferences");
    }

   public interface OkListener {
        public void okPressed();
    }

    public void setOkListener(OkListener okListener) {
    	this.okListener = okListener;
    }

	public void setModeOff() {
		hpilOffRB.setChecked(true);
		setModeAvailables();
	}
	
	public void setModeIP() {
		hpilModeIPRB.setChecked(true);
		setModeAvailables();
	}
	
	public boolean getModeIP() {
		return hpilModeIPRB.isChecked();
	}
	
	public void setModePilBox() {
		hpilModePilBoxRB.setChecked(true);
		setModeAvailables();
	}
	
	public boolean getModePilBox() {
		return hpilModePilBoxRB.isChecked();
	}
	
	public void setModeBluetooth() {
		hpilModeBluetoothRB.setChecked(true);
		setModeAvailables();
		}
	
	public boolean getModeBluetooth() {
		return hpilModeBluetoothRB.isChecked();
	}

	public void setBtPairing(String pair) {
		int i;
		for (i = 0; i < hpilBTPairingSP.getCount(); i++) {
			if (hpilBTPairingSP.getItemAtPosition(i).toString().equals(pair)) {
				hpilBTPairingSP.setSelection(i);
				break;
			}
		}
	}
	
	public String getBtPairing() {
		String btPaired = "";
		try {
		btPaired = hpilBTPairingSP.getSelectedItem().toString();
		}
		catch (Exception e) {
		}
		return btPaired;
	}
	
	public void setLatency(String latency) {
		int i;
		for (i = 0; i < hpilLatencySP.getCount(); i++) {
			if (hpilLatencySP.getItemAtPosition(i).toString().equals(latency)) {
				hpilLatencySP.setSelection(i);
				break;
			}
		}
	}
	
	public String getLatency() {
		return hpilLatencySP.getSelectedItem().toString();
	}
	public void setIPAddr(String addr) {
		hpilAddrET.setText(addr);
	}
	
	public String getIPAddr() {
		return hpilAddrET.getText().toString();
	}
	
	public void setOutPort(int port) {
		hpilOutPortET.setText(Integer.toString(port));
	}
	
	public int getOutPort() {
		return Integer.parseInt(hpilOutPortET.getText().toString());
	}
	
	public void setInPort(int port) {
		hpilInPortET.setText(Integer.toString(port));
	}
	
	public int getInPort() {
		return Integer.parseInt(hpilInPortET.getText().toString());
	}

	public void setDebug(boolean state) {
		hpilDebugCB.setChecked(state);
	}
	
	public boolean getDebug() {
		return hpilDebugCB.isChecked();
	}
	
	private void setModeAvailables() {
		if (hpilModeIPRB.isChecked()) {
			hpilBTPairingSP.setEnabled(false);
			hpilAddrET.setEnabled(true);
			hpilOutPortET.setEnabled(true);
			hpilInPortET.setEnabled(true);
		}
		else if (hpilModePilBoxRB.isChecked() || hpilModeBluetoothRB.isChecked()) {
			hpilBTPairingSP.setEnabled(true);
			hpilAddrET.setEnabled(false);
			hpilOutPortET.setEnabled(false);
			hpilInPortET.setEnabled(false);
		}
		else {
			hpilBTPairingSP.setEnabled(false);
			hpilAddrET.setEnabled(false);
			hpilOutPortET.setEnabled(false);
			hpilInPortET.setEnabled(false);
		}
	}
	
}
