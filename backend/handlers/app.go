package handlers

import (
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"cloud.google.com/go/firestore"
	"google.golang.org/api/iterator"

	"iot-smart-connector/backend/config"
	"iot-smart-connector/backend/middleware"
	"iot-smart-connector/backend/models"
)

type CreateAppRequest struct {
	Name                 string          `json:"name"`
	BrandingLogoURL      string          `json:"branding_logo_url"`
	BrandingPrimaryColor string          `json:"branding_primary_color"`
	CustomFieldsSchema   json.RawMessage `json:"custom_fields_schema"`
}

func CreateApp(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error": "Method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	publisherID, ok := r.Context().Value(middleware.PublisherIDKey).(string)
	if !ok || publisherID == "" {
		http.Error(w, `{"error": "Unauthorized"}`, http.StatusUnauthorized)
		return
	}

	var req CreateAppRequest
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		http.Error(w, `{"error": "Invalid request body"}`, http.StatusBadRequest)
		return
	}

	if req.Name == "" {
		http.Error(w, `{"error": "App Name is required"}`, http.StatusBadRequest)
		return
	}

	if req.BrandingPrimaryColor == "" {
		req.BrandingPrimaryColor = "#00b4d8"
	}

	schemaJSON := "[]"
	if len(req.CustomFieldsSchema) > 0 {
		schemaJSON = string(req.CustomFieldsSchema)
	}

	ctx := r.Context()

	// Generate dynamic BLE short Hex ID (loop to ensure uniqueness)
	var bleID string
	for {
		bleID, err = models.GenerateShortHexID()
		if err != nil {
			http.Error(w, `{"error": "Failed to generate BLE ID"}`, http.StatusInternalServerError)
			return
		}

		iterCollision := config.FirestoreClient.Collection("apps").Where("ble_id", "==", bleID).Limit(1).Documents(ctx)
		_, errCollision := iterCollision.Next()
		if errCollision == iterator.Done {
			// Unique BLE ID found
			break
		}
		if errCollision != nil {
			http.Error(w, fmt.Sprintf(`{"error": "Database error checking BLE ID: %v"}`, errCollision), http.StatusInternalServerError)
			return
		}
	}

	docRef := config.FirestoreClient.Collection("apps").NewDoc()
	app := models.App{
		ID:                   docRef.ID,
		PublisherID:          publisherID,
		Name:                 req.Name,
		BleID:                bleID,
		BrandingLogoURL:      req.BrandingLogoURL,
		BrandingPrimaryColor: req.BrandingPrimaryColor,
		CustomFieldsSchema:   schemaJSON,
		CreatedAt:            time.Now(),
	}

	_, err = docRef.Set(ctx, app)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Failed to create app: %v"}`, err), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusCreated)
	json.NewEncoder(w).Encode(app)
}

func ListApps(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error": "Method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	publisherID, ok := r.Context().Value(middleware.PublisherIDKey).(string)
	if !ok || publisherID == "" {
		http.Error(w, `{"error": "Unauthorized"}`, http.StatusUnauthorized)
		return
	}

	ctx := r.Context()

	// Retrieve apps ordered by CreatedAt desc
	iter := config.FirestoreClient.Collection("apps").
		Where("publisher_id", "==", publisherID).
		OrderBy("created_at", firestore.Desc).
		Documents(ctx)

	apps := []models.App{}
	for {
		doc, err := iter.Next()
		if err == iterator.Done {
			break
		}
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error": "Database error listing apps: %v"}`, err), http.StatusInternalServerError)
			return
		}
		var app models.App
		if err := doc.DataTo(&app); err != nil {
			http.Error(w, fmt.Sprintf(`{"error": "Error parsing app data: %v"}`, err), http.StatusInternalServerError)
			return
		}
		apps = append(apps, app)
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(apps)
}

func DeleteApp(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodDelete {
		http.Error(w, `{"error": "Method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	publisherID, ok := r.Context().Value(middleware.PublisherIDKey).(string)
	if !ok || publisherID == "" {
		http.Error(w, `{"error": "Unauthorized"}`, http.StatusUnauthorized)
		return
	}

	appID := r.PathValue("id")
	if appID == "" {
		http.Error(w, `{"error": "App ID is required"}`, http.StatusBadRequest)
		return
	}

	ctx := r.Context()

	// Verify ownership
	docRef := config.FirestoreClient.Collection("apps").Doc(appID)
	docSnap, err := docRef.Get(ctx)
	if err != nil {
		http.Error(w, `{"error": "App not found or permission denied"}`, http.StatusNotFound)
		return
	}

	var app models.App
	if err := docSnap.DataTo(&app); err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Error reading app data: %v"}`, err), http.StatusInternalServerError)
		return
	}

	if app.PublisherID != publisherID {
		http.Error(w, `{"error": "App not found or permission denied"}`, http.StatusNotFound)
		return
	}

	// Delete app document
	_, err = docRef.Delete(ctx)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Failed to delete app: %v"}`, err), http.StatusInternalServerError)
		return
	}

	// Delete cascaded activations
	actIter := config.FirestoreClient.Collection("activations").Where("app_id", "==", appID).Documents(ctx)
	for {
		actDoc, err := actIter.Next()
		if err == iterator.Done {
			break
		}
		if err != nil {
			break // Non-blocking
		}
		actDoc.Ref.Delete(ctx)
	}

	w.WriteHeader(http.StatusNoContent)
}

func GetAppActivations(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		http.Error(w, `{"error": "Method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	publisherID, ok := r.Context().Value(middleware.PublisherIDKey).(string)
	if !ok || publisherID == "" {
		http.Error(w, `{"error": "Unauthorized"}`, http.StatusUnauthorized)
		return
	}

	appID := r.PathValue("id")
	if appID == "" {
		http.Error(w, `{"error": "App ID is required"}`, http.StatusBadRequest)
		return
	}

	ctx := r.Context()

	// Verify owner
	docSnap, err := config.FirestoreClient.Collection("apps").Doc(appID).Get(ctx)
	if err != nil {
		http.Error(w, `{"error": "App not found or permission denied"}`, http.StatusNotFound)
		return
	}

	var app models.App
	if err := docSnap.DataTo(&app); err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Error parsing app details: %v"}`, err), http.StatusInternalServerError)
		return
	}

	if app.PublisherID != publisherID {
		http.Error(w, `{"error": "App not found or permission denied"}`, http.StatusNotFound)
		return
	}

	// Retrieve activations
	iter := config.FirestoreClient.Collection("activations").
		Where("app_id", "==", appID).
		OrderBy("activated_at", firestore.Desc).
		Documents(ctx)

	activations := []models.Activation{}
	for {
		doc, err := iter.Next()
		if err == iterator.Done {
			break
		}
		if err != nil {
			http.Error(w, fmt.Sprintf(`{"error": "Database error fetching activations: %v"}`, err), http.StatusInternalServerError)
			return
		}
		var act models.Activation
		if err := doc.DataTo(&act); err != nil {
			http.Error(w, fmt.Sprintf(`{"error": "Error parsing activation details: %v"}`, err), http.StatusInternalServerError)
			return
		}
		activations = append(activations, act)
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(activations)
}

type UpdateAppRequest struct {
	Name                 string          `json:"name"`
	BrandingLogoURL      string          `json:"branding_logo_url"`
	BrandingPrimaryColor string          `json:"branding_primary_color"`
	CustomFieldsSchema   json.RawMessage `json:"custom_fields_schema"`
}

func UpdateApp(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPut {
		http.Error(w, `{"error": "Method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	publisherID, ok := r.Context().Value(middleware.PublisherIDKey).(string)
	if !ok || publisherID == "" {
		http.Error(w, `{"error": "Unauthorized"}`, http.StatusUnauthorized)
		return
	}

	appID := r.PathValue("id")
	if appID == "" {
		http.Error(w, `{"error": "App ID is required"}`, http.StatusBadRequest)
		return
	}

	ctx := r.Context()

	// Verify owner
	docRef := config.FirestoreClient.Collection("apps").Doc(appID)
	docSnap, err := docRef.Get(ctx)
	if err != nil {
		http.Error(w, `{"error": "App not found or permission denied"}`, http.StatusNotFound)
		return
	}

	var app models.App
	if err := docSnap.DataTo(&app); err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Error reading app data: %v"}`, err), http.StatusInternalServerError)
		return
	}

	if app.PublisherID != publisherID {
		http.Error(w, `{"error": "App not found or permission denied"}`, http.StatusNotFound)
		return
	}

	var req UpdateAppRequest
	err = json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		http.Error(w, `{"error": "Invalid request body"}`, http.StatusBadRequest)
		return
	}

	if req.Name == "" {
		http.Error(w, `{"error": "App Name is required"}`, http.StatusBadRequest)
		return
	}

	if req.BrandingPrimaryColor == "" {
		req.BrandingPrimaryColor = "#00b4d8"
	}

	schemaJSON := "[]"
	if len(req.CustomFieldsSchema) > 0 {
		schemaJSON = string(req.CustomFieldsSchema)
	}

	// Update app
	app.Name = req.Name
	app.BrandingLogoURL = req.BrandingLogoURL
	app.BrandingPrimaryColor = req.BrandingPrimaryColor
	app.CustomFieldsSchema = schemaJSON

	_, err = docRef.Set(ctx, app)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Failed to update app: %v"}`, err), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(app)
}
