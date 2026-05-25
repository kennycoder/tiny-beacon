package handlers

import (
	"encoding/json"
	"fmt"
	"net/http"
	"time"

	"iot-smart-connector/backend/config"
	"iot-smart-connector/backend/middleware"
	"iot-smart-connector/backend/models"

	"github.com/golang-jwt/jwt/v5"
	"golang.org/x/crypto/bcrypt"
	"google.golang.org/api/iterator"
)

type RegisterRequest struct {
	Name     string `json:"name"`
	Email    string `json:"email"`
	Password string `json:"password"`
}

type LoginRequest struct {
	Email    string `json:"email"`
	Password string `json:"password"`
}

type AuthResponse struct {
	Token     string           `json:"token"`
	Publisher models.Publisher `json:"publisher"`
}

func Register(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error": "Method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	var req RegisterRequest
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		http.Error(w, `{"error": "Invalid request body"}`, http.StatusBadRequest)
		return
	}

	if req.Name == "" || req.Email == "" || req.Password == "" {
		http.Error(w, `{"error": "Name, Email, and Password are required"}`, http.StatusBadRequest)
		return
	}

	ctx := r.Context()

	// Check if email already exists
	iter := config.FirestoreClient.Collection("publishers").Where("email", "==", req.Email).Limit(1).Documents(ctx)
	_, err = iter.Next()
	if err != iterator.Done {
		if err == nil {
			http.Error(w, `{"error": "Email is already registered"}`, http.StatusConflict)
			return
		}
		http.Error(w, fmt.Sprintf(`{"error": "Database error checking email: %v"}`, err), http.StatusInternalServerError)
		return
	}

	// Hash password
	hashedPassword, err := bcrypt.GenerateFromPassword([]byte(req.Password), bcrypt.DefaultCost)
	if err != nil {
		http.Error(w, `{"error": "Failed to hash password"}`, http.StatusInternalServerError)
		return
	}

	// Generate dynamic BLE short Hex ID (loop to ensure uniqueness)
	var bleID string
	for {
		bleID, err = models.GenerateShortHexID()
		if err != nil {
			http.Error(w, `{"error": "Failed to generate BLE ID"}`, http.StatusInternalServerError)
			return
		}

		iterCollision := config.FirestoreClient.Collection("publishers").Where("ble_id", "==", bleID).Limit(1).Documents(ctx)
		_, errCollision := iterCollision.Next()
		if errCollision == iterator.Done {
			// No collision found, we can use this BLE ID
			break
		}
		if errCollision != nil {
			http.Error(w, fmt.Sprintf(`{"error": "Database error checking BLE ID collision: %v"}`, errCollision), http.StatusInternalServerError)
			return
		}
	}

	// Create document in Firestore
	docRef := config.FirestoreClient.Collection("publishers").NewDoc()
	pub := models.Publisher{
		ID:           docRef.ID,
		Name:         req.Name,
		Email:        req.Email,
		PasswordHash: string(hashedPassword),
		BleID:        bleID,
		CreatedAt:    time.Now(),
	}

	_, err = docRef.Set(ctx, pub)
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Failed to register publisher: %v"}`, err), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusCreated)
	json.NewEncoder(w).Encode(pub)
}

func Login(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, `{"error": "Method not allowed"}`, http.StatusMethodNotAllowed)
		return
	}

	var req LoginRequest
	err := json.NewDecoder(r.Body).Decode(&req)
	if err != nil {
		http.Error(w, `{"error": "Invalid request body"}`, http.StatusBadRequest)
		return
	}

	ctx := r.Context()

	// Retrieve publisher
	iter := config.FirestoreClient.Collection("publishers").Where("email", "==", req.Email).Limit(1).Documents(ctx)
	doc, err := iter.Next()
	if err == iterator.Done {
		http.Error(w, `{"error": "Invalid email or password"}`, http.StatusUnauthorized)
		return
	}
	if err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Database error: %v"}`, err), http.StatusInternalServerError)
		return
	}

	var pub models.Publisher
	if err := doc.DataTo(&pub); err != nil {
		http.Error(w, fmt.Sprintf(`{"error": "Error decoding publisher: %v"}`, err), http.StatusInternalServerError)
		return
	}

	// Verify password
	err = bcrypt.CompareHashAndPassword([]byte(pub.PasswordHash), []byte(req.Password))
	if err != nil {
		http.Error(w, `{"error": "Invalid email or password"}`, http.StatusUnauthorized)
		return
	}

	// Create JWT token
	expirationTime := time.Now().Add(24 * time.Hour)
	claims := &middleware.Claims{
		PublisherID: pub.ID,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(expirationTime),
			IssuedAt:  jwt.NewNumericDate(time.Now()),
		},
	}

	token := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	tokenStr, err := token.SignedString(middleware.JwtSecret)
	if err != nil {
		http.Error(w, `{"error": "Failed to generate token"}`, http.StatusInternalServerError)
		return
	}

	// Respond
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(AuthResponse{
		Token:     tokenStr,
		Publisher: pub,
	})
}
