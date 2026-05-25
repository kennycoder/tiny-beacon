package handlers

import (
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"google.golang.org/api/iterator"

	"iot-smart-connector/backend/config"
	"iot-smart-connector/backend/models"
)

type ResolveResponse struct {
	PublisherName        string          `json:"publisher_name"`
	AppName              string          `json:"app_name"`
	AppID                string          `json:"app_id"`
	BrandingLogoURL      string          `json:"branding_logo_url"`
	BrandingPrimaryColor string          `json:"branding_primary_color"`
	CustomFieldsSchema   json.RawMessage `json:"custom_fields_schema"`
}

type ActivateRequest struct {
	AppID              string          `json:"app_id"` // Can be BLE short ID or full UUID
	DeviceMac          string          `json:"device_mac"`
	CustomFieldsValues json.RawMessage `json:"custom_fields_values"`
}

func ResolveApp(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error": "Method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	publisherBleID := r.URL.Query().Get("publisher_id")
	appBleID := r.URL.Query().Get("app_id")

	if publisherBleID == "" || appBleID == "" {
		http.Error(w, `{"error": "publisher_id and app_id query params are required"}`, http.StatusBadRequest)
		return
	}

	ctx := r.Context()

	// 1. Resolve publisher by BleID
	pubIter := config.FirestoreClient.Collection("publishers").Where("ble_id", "==", publisherBleID).Limit(1).Documents(ctx)
	pubDoc, err := pubIter.Next()
	if err == iterator.Done {
		http.Error(w, `{"error": "Publisher or App not found"}`, http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Database error resolving publisher: %v"}`, err), http.StatusInternalServerError)
		return
	}

	var pub models.Publisher
	if err := pubDoc.DataTo(&pub); err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Error parsing publisher details: %v"}`, err), http.StatusInternalServerError)
		return
	}

	// 2. Resolve app by BleID and PublisherID
	appIter := config.FirestoreClient.Collection("apps").
		Where("ble_id", "==", appBleID).
		Where("publisher_id", "==", pub.ID).
		Limit(1).
		Documents(ctx)
	appDoc, err := appIter.Next()
	if err == iterator.Done {
		http.Error(w, `{"error": "Publisher or App not found"}`, http.StatusNotFound)
		return
	}
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Database error resolving app: %v"}`, err), http.StatusInternalServerError)
		return
	}

	var app models.App
	if err := appDoc.DataTo(&app); err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Error parsing app details: %v"}`, err), http.StatusInternalServerError)
		return
	}

	schemaJSON := []byte("[]")
	if app.CustomFieldsSchema != "" {
		schemaJSON = []byte(app.CustomFieldsSchema)
	}

	res := ResolveResponse{
		PublisherName:        pub.Name,
		AppName:              app.Name,
		AppID:                app.ID,
		BrandingLogoURL:      app.BrandingLogoURL,
		BrandingPrimaryColor: app.BrandingPrimaryColor,
		CustomFieldsSchema:   json.RawMessage(schemaJSON),
	}

	w.Header().Set("Access-Control-Allow-Origin", "*") // For Web Simulator CORS
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(res)
}

func ActivateDevice(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error": "Method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	var req ActivateRequest
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		http.Error(w, `{"error": "Invalid request body"}`, http.StatusBadRequest)
		return
	}

	if req.AppID == "" || req.DeviceMac == "" {
		http.Error(w, `{"error": "app_id and device_mac are required"}`, http.StatusBadRequest)
		return
	}

	ctx := r.Context()

	// Find the database App UUID if BLE ID was provided
	var appUUID string
	if len(req.AppID) == 4 {
		appIter := config.FirestoreClient.Collection("apps").Where("ble_id", "==", req.AppID).Limit(1).Documents(ctx)
		appDoc, err := appIter.Next()
		if err == iterator.Done {
			http.Error(w, `{"error": "App not found"}`, http.StatusNotFound)
			return
		}
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error": "Database error: %v"}`, err), http.StatusInternalServerError)
			return
		}
		var app models.App
		if err := appDoc.DataTo(&app); err != nil {
			http.Error(w, fmt.Sprintf(`{"error": "Error reading app: %v"}`, err), http.StatusInternalServerError)
			return
		}
		appUUID = app.ID
	} else {
		appDoc, err := config.FirestoreClient.Collection("apps").Doc(req.AppID).Get(ctx)
		if err != nil {
			http.Error(w, `{"error": "App not found"}`, http.StatusNotFound)
			return
		}
		var app models.App
		if err := appDoc.DataTo(&app); err != nil {
			http.Error(w, fmt.Sprintf(`{"error": "Error reading app: %v"}`, err), http.StatusInternalServerError)
			return
		}
		appUUID = app.ID
	}

	valuesJSON := "{}"
	if len(req.CustomFieldsValues) > 0 {
		valuesJSON = string(req.CustomFieldsValues)
	}

	// Insert activation record
	docRefAct := config.FirestoreClient.Collection("activations").NewDoc()
	act := models.Activation{
		ID:                 docRefAct.ID,
		AppID:              appUUID,
		DeviceMac:          req.DeviceMac,
		CustomFieldsValues: valuesJSON,
		ActivatedAt:        time.Now(),
	}

	_, err = docRefAct.Set(ctx, act)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Failed to log activation: %v"}`, err), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Access-Control-Allow-Origin", "*") // For Web Simulator CORS
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusCreated)
	json.NewEncoder(w).Encode(act)
}
