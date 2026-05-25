package models

import (
	"crypto/rand"
	"encoding/hex"
	"time"
)

type Publisher struct {
	ID           string    `json:"id" firestore:"id"`
	Name         string    `json:"name" firestore:"name"`
	Email        string    `json:"email" firestore:"email"`
	PasswordHash string    `json:"-" firestore:"password_hash"`
	BleID        string    `json:"ble_id" firestore:"ble_id"`
	CreatedAt    time.Time `json:"created_at" firestore:"created_at"`
}

type App struct {
	ID                   string    `json:"id" firestore:"id"`
	PublisherID          string    `json:"publisher_id" firestore:"publisher_id"`
	Name                 string    `json:"name" firestore:"name"`
	BleID                string    `json:"ble_id" firestore:"ble_id"`
	BrandingLogoURL      string    `json:"branding_logo_url" firestore:"branding_logo_url"`
	BrandingPrimaryColor string    `json:"branding_primary_color" firestore:"branding_primary_color"`
	CustomFieldsSchema   string    `json:"custom_fields_schema" firestore:"custom_fields_schema"` // String representation of raw JSONB
	CreatedAt            time.Time `json:"created_at" firestore:"created_at"`
}

type Activation struct {
	ID                 string    `json:"id" firestore:"id"`
	AppID              string    `json:"app_id" firestore:"app_id"`
	DeviceMac          string    `json:"device_mac" firestore:"device_mac"`
	CustomFieldsValues string    `json:"custom_fields_values" firestore:"custom_fields_values"` // String representation of raw JSONB
	ActivatedAt        time.Time `json:"activated_at" firestore:"activated_at"`
}

// GenerateShortHexID generates a unique 4-character hex ID (like "a1b2")
func GenerateShortHexID() (string, error) {
	bytes := make([]byte, 2)
	if _, err := rand.Read(bytes); err != nil {
		return "", err
	}
	return hex.EncodeToString(bytes), nil
}
